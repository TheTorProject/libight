// Part of measurement-kit <https://measurement-kit.github.io/>.
// Measurement-kit is free software under the BSD license. See AUTHORS
// and LICENSE for more information on the copying conditions.

#include "private/net/connect.hpp"
#include "private/net/socks5.hpp"
#include <measurement_kit/net.hpp>

namespace mk {
namespace net {

static void socks5_connect_(SharedPtr<Transport> conn, Settings settings,
        SharedPtr<Reactor> reactor, SharedPtr<Logger> logger,
        Callback<Error, SharedPtr<Transport>> &&cb);

Buffer socks5_format_auth_request(SharedPtr<Logger> logger) {
    Buffer out;
    out.write_uint8(5); // Version
    out.write_uint8(1); // Number of methods
    out.write_uint8(0); // "NO_AUTH" meth.
    logger->debug("socks5: >> version=5");
    logger->debug("socks5: >> number of methods=1");
    logger->debug("socks5: >> NO_AUTH (0)");
    return out;
}

ErrorOr<bool> socks5_parse_auth_response(
        Buffer &buffer, SharedPtr<Logger> logger) {
    auto readbuf = buffer.readn(2);
    if (readbuf == "") {
        return false; // Try again after next recv()
    }
    logger->debug("socks5: << version=%d", readbuf[0]);
    logger->debug("socks5: << preferred_auth=%d", readbuf[1]);
    if (readbuf[0] != 5) {
        return BadSocksVersionError();
    }
    if (readbuf[1] != 0) {
        return NoAvailableSocksAuthenticationError();
    }
    return true;
}

ErrorOr<Buffer> socks5_format_connect_request(
        Settings settings, SharedPtr<Logger> logger) {
    Buffer out;

    out.write_uint8(5); // Version
    out.write_uint8(1); // CMD_CONNECT
    out.write_uint8(0); // Reserved
    out.write_uint8(3); // ATYPE_DOMAINNAME

    logger->debug("socks5: >> version=5");
    logger->debug("socks5: >> CMD_CONNECT (1)");
    logger->debug("socks5: >> Reserved (0)");
    logger->debug("socks5: >> ATYPE_DOMAINNAME (3)");

    auto address = settings["_socks5/address"];

    if (address.length() > 255) {
        return SocksAddressTooLongError();
    }
    out.write_uint8(address.length());            // Len
    out.write(address.c_str(), address.length()); // String

    logger->debug("socks5: >> domain len=%d", (uint8_t)address.length());
    logger->debug("socks5: >> domain str=%s", address.c_str());

    int portnum = settings["_socks5/port"].as<int>();
    if (portnum < 0 || portnum > 65535) {
        return SocksInvalidPortError();
    }
    out.write_uint16(portnum); // Port

    logger->debug("socks5: >> port=%d", portnum);

    return out;
}

ErrorOr<bool> socks5_parse_connect_response(
        Buffer &buffer, SharedPtr<Logger> logger) {
    if (buffer.length() < 5) {
        return false; // Try again after next recv()
    }

    auto peekbuf = buffer.peek(5);

    logger->debug("socks5: << version=%d", peekbuf[0]);
    logger->debug("socks5: << reply=%d", peekbuf[1]);
    logger->debug("socks5: << reserved=%d", peekbuf[2]);
    logger->debug("socks5: << atype=%d", peekbuf[3]);

    if (peekbuf[0] != 5) {
        return BadSocksVersionError();
    }
    if (peekbuf[1] != 0) {
        return SocksError(); // TODO: also return the actual error
    }
    if (peekbuf[2] != 0) {
        return BadSocksReservedFieldError();
    }

    auto atype = peekbuf[3]; // Atype

    size_t total = 4; // Version .. Atype size
    if (atype == 1) {
        total += 4; // IPv4 addr size
    } else if (atype == 3) {
        total += 1 + peekbuf[4]; // Len size + String size
    } else if (atype == 4) {
        total += 16; // IPv6 addr size
    } else {
        return BadSocksAtypeValueError();
    }
    total += 2; // Port size
    if (buffer.length() < total) {
        return false; // Try again after next recv()
    }

    buffer.discard(total);
    return true;
}

void socks5_connect(std::string address, int port, Settings settings,
        Callback<Error, SharedPtr<Transport>> callback,
        SharedPtr<Reactor> reactor, SharedPtr<Logger> logger) {

    auto proxy = settings["net/socks5_proxy"];
    auto pos = proxy.find(":");
    if (pos == std::string::npos) {
        throw std::runtime_error("invalid argument");
    }
    auto proxy_address = proxy.substr(0, pos);
    auto proxy_port = proxy.substr(pos + 1);

    // We must erase this setting because we're about to call net::connect
    // again and that would loop unless we remove this setting.
    settings.erase("net/socks5_proxy");

    // Store the address and the port we must connect through SOCKS5
    settings["_socks5/address"] = address;
    settings["_socks5/port"] = port;

    // When SSL is requested, we must establish it with the remote server
    // and not with the SOCKS5 server, so move the key to a private name
    // used only by us: we will take care of it later.
    if (settings.count("net/ssl") != 0) {
        settings["_socks5/ssl"] = settings["net/ssl"];
        settings.erase("net/ssl");
    }

    net::connect(proxy_address, lexical_cast<int>(proxy_port),
            [=](Error err, SharedPtr<Transport> txp) mutable {
                if (err) {
                    callback(err, txp);
                    return;
                }
                socks5_connect_(
                        txp, settings, reactor, logger, std::move(callback));
            },
            settings, reactor, logger);
}

static void socks5_connect_(SharedPtr<Transport> conn, Settings settings,
        SharedPtr<Reactor> reactor, SharedPtr<Logger> logger,
        Callback<Error, SharedPtr<Transport>> &&cb) {
    // Step #1: send out preferred authentication methods

    logger->debug("socks5: connected to Tor!");
    conn->write(socks5_format_auth_request(logger));
    SharedPtr<Buffer> buffer = Buffer::make();

    // Step #2: receive the allowed authentication methods

    conn->on_data([=](Buffer d) {
        *buffer << d;
        ErrorOr<bool> result = socks5_parse_auth_response(*buffer, logger);
        if (!result) {
            cb(result.as_error(), conn);
            return;
        }
        if (!*result) {
            return; // continue reading
        }

        // Step #3: ask Tor to connect to remote host

        ErrorOr<Buffer> out = socks5_format_connect_request(settings, logger);
        if (!out) {
            cb(out.as_error(), conn);
            return;
        }
        conn->write(*out);

        // Step #4: receive Tor's response

        conn->on_data([=](Buffer d) {
            *buffer << d;
            ErrorOr<bool> rc = socks5_parse_connect_response(*buffer, logger);
            if (!rc) {
                cb(rc.as_error(), conn);
                return;
            }
            if (!*rc) {
                return; // continue reading
            }

            //
            // Step #5: we are now connected
            // Restore the original hooks
            // Tell upstream we are connected
            // If more data, pass it up
            //

            conn->on_flush(nullptr);
            conn->on_data(nullptr);
            conn->on_error(nullptr);

            ErrorOr<bool> ssl = settings.get_noexcept("_socks5/ssl", false);
            if (!ssl) {
                cb(ssl.as_error(), conn);
                return;
            }
            if (!*ssl) {
                cb(NoError(), conn);
                return;
            }

            connect_ssl(conn, settings.at("_socks5/address"), settings,
                    reactor, logger, [=](Error err) {
                cb(err, conn);
            });

#if 0
            // Note that emit_connect() may have called close() but even
            // in such case, emit_data() is a NO-OP if connection is closed
            if (buffer->length() > 0) {
                // FIXME This should probably be reimplemented without using the
                // bufferevent for reading because the bufferevent has the problem
                // that we may read more than expected and then, if this happens,
                // and we need to establish SSL, it's not clear to me how to do this.
                /*conn->emit_data(*buffer);*/
            }
#endif
        });
    });
}

} // namespace net
} // namespace mk
