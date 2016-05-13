// Part of measurement-kit <https://measurement-kit.github.io/>.
// Measurement-kit is free software. See AUTHORS and LICENSE for more
// information on the copying conditions.
#ifndef SRC_NDT_PROTOCOL_IMPL_HPP
#define SRC_NDT_PROTOCOL_IMPL_HPP

#include "src/ndt/internal.hpp"

namespace mk {
namespace ndt {
namespace protocol {

template <MK_MOCK_NAMESPACE(net, connect)>
void connect_impl(Var<Context> ctx, Callback<Error> callback) {
    ctx->logger->debug("ndt: connect ...");
    connect(ctx->address, ctx->port,
            [=](Error err, Var<Transport> txp) {
                ctx->logger->debug("ndt: connect ... %d", (int)err);
                if (err) {
                    callback(err);
                    return;
                }
                // FIXME: make sure timeout mechanism is sane
                txp->set_timeout(60.0);
                ctx->txp = txp;
                ctx->logger->info("Connected to %s:%d", ctx->address.c_str(),
                                  ctx->port);
                callback(NoError());
            },
            ctx->settings, ctx->logger, ctx->reactor);
}

template <MK_MOCK_NAMESPACE(messages, format_msg_extended_login),
          MK_MOCK_NAMESPACE(messages, write)>
void send_extended_login_impl(Var<Context> ctx, Callback<Error> callback) {
    ctx->logger->debug("ndt: send login ...");
    ErrorOr<Buffer> out = format_msg_extended_login(ctx->test_suite);
    if (!out) {
        ctx->logger->debug("ndt: send login ... %d", (int)out.as_error());
        callback(out.as_error());
        return;
    }
    write(ctx, *out, [=](Error err) {
        ctx->logger->debug("ndt: send login ... %d", (int)err);
        if (err) {
            callback(err);
            return;
        }
        ctx->logger->info("Sent LOGIN with test suite: %d", ctx->test_suite);
        callback(NoError());
    });
}

template <MK_MOCK_NAMESPACE(net, readn)>
void recv_and_ignore_kickoff_impl(Var<Context> ctx, Callback<Error> callback) {
    ctx->logger->debug("ndt: recv and ignore kickoff ...");
    readn(ctx->txp, ctx->buff, KICKOFF_MESSAGE_SIZE, [=](Error err) {
        ctx->logger->debug("ndt: recv and ignore kickoff ... %d", (int)err);
        if (err) {
            callback(err);
            return;
        }
        std::string s = ctx->buff->readn(KICKOFF_MESSAGE_SIZE);
        if (s != KICKOFF_MESSAGE) {
            // TODO: wouldn't it better to just warn?
            callback(GenericError());
            return;
        }
        ctx->logger->info("Got legacy KICKOFF message (ignored)");
        callback(NoError());
    });
}

template <MK_MOCK_NAMESPACE(messages, read)>
void wait_in_queue_impl(Var<Context> ctx, Callback<Error> callback) {
    ctx->logger->debug("ndt: wait in queue ...");
    read(ctx, [=](Error err, uint8_t type, std::string s) {
        ctx->logger->debug("ndt: wait in queue ... %d", (int)err);
        if (err) {
            callback(err);
            return;
        }
        if (type != SRV_QUEUE) {
            callback(GenericError());
            return;
        }
        ErrorOr<unsigned> wait_time = lexical_cast_noexcept<unsigned>(s);
        if (!wait_time) {
            callback(wait_time.as_error());
            return;
        }
        ctx->logger->info("Wait time before test starts: %d", *wait_time);
        // XXX Simplified implementation
        if (*wait_time > 0) {
            callback(GenericError());
            return;
        }
        callback(NoError());
    });
}

template <MK_MOCK_NAMESPACE(messages, read)>
void recv_version_impl(Var<Context> ctx, Callback<Error> callback) {
    ctx->logger->debug("ndt: recv server version ...");
    read(ctx, [=](Error err, uint8_t type, std::string s) {
        ctx->logger->debug("ndt: recv server version ... %d", (int)err);
        if (err) {
            callback(err);
            return;
        }
        if (type != MSG_LOGIN) {
            callback(GenericError());
            return;
        }
        ctx->logger->info("Got server version: %s", s.c_str());
        // TODO: validate the server version?
        callback(NoError());
    });
}

template <MK_MOCK_NAMESPACE(messages, read)>
void recv_tests_id_impl(Var<Context> ctx, Callback<Error> callback) {
    ctx->logger->debug("ndt: recv tests ID ...");
    read(ctx, [=](Error err, uint8_t type, std::string s) {
        ctx->logger->debug("ndt: recv tests ID ... %d", (int)err);
        if (err) {
            callback(err);
            return;
        }
        if (type != MSG_LOGIN) {
            callback(GenericError());
            return;
        }
        ctx->logger->info("Authorized tests: %s", s.c_str());
        ctx->granted_suite = split(s);
        callback(NoError());
    });
}

template <MK_MOCK_NAMESPACE(messages, read_json),
          MK_MOCK_NAMESPACE(messages, format_test_msg)>
void run_tests_impl(Var<Context> ctx, Callback<Error> callback) {

    if (ctx->granted_suite.size() <= 0) {
        callback(NoError());
        return;
    }

    std::string s = ctx->granted_suite.front();
    ctx->granted_suite.pop_front();

    ErrorOr<int> num = lexical_cast<int>(s);
    if (!num) {
        callback(num.as_error());
        return;
    }

    if (*num == TEST_C2S) {
        ctx->logger->info("Run C2S test...");
        test_c2s::run(ctx, [=](Error err) {
            ctx->logger->info("Run C2S test... complete (%d)", (int)err);
            if (err) {
                callback(err);
                return;
            }
            run_tests_impl<read_json, format_test_msg>(ctx, callback);
        });
        return;
    }

    if (*num == TEST_S2C) {
        ctx->logger->info("Run S2C test...");
        test_s2c::run(ctx, [=](Error err) {
            ctx->logger->info("Run S2C test... complete (%d)", (int)err);
            if (err) {
                callback(err);
                return;
            }
            run_tests_impl<read_json, format_test_msg>(ctx, callback);
        });
        return;
    }

    if (*num == TEST_META) {
        ctx->logger->info("Run META test...");
        test_meta::run(ctx, [=](Error err) {
            ctx->logger->info("Run META test... complete (%d)", (int)err);
            if (err) {
                callback(err);
                return;
            }
            run_tests_impl<read_json, format_test_msg>(ctx, callback);
        });
        return;
    }

    ctx->logger->warn("ndt: unknown test: %d", *num);
    callback(GenericError());
}

template <MK_MOCK_NAMESPACE(messages, read)>
void recv_results_and_logout_impl(Var<Context> ctx, Callback<Error> callback) {
    ctx->logger->debug("ndt: recv RESULTS ...");
    read(ctx, [=](Error err, uint8_t type, std::string s) {
        ctx->logger->debug("ndt: recv RESULTS ... %d", (int)err);
        if (err) {
            callback(err);
            return;
        }
        if (type == MSG_RESULTS) {
            for (auto x : split(s, "\n")) {
                if (x != "") {
                    // Should be info because it allows us to see the
                    // internals at the end of the NDT test
                    ctx->logger->info("%s", x.c_str());
                }
            }
            // XXX: here we have the potential to loop forever...
            recv_results_and_logout_impl<read>(ctx, callback);
            return;
        }
        if (type != MSG_LOGOUT) {
            callback(GenericError());
            return;
        }
        ctx->logger->info("Got LOGOUT");
        callback(NoError());
    });
}

template <MK_MOCK_NAMESPACE(net, read)>
void wait_close_impl(Var<Context> ctx, Callback<Error> callback) {
    ctx->logger->debug("ndt: wait close ...");
    ctx->txp->set_timeout(1.0);
    Var<Buffer> buffer(new Buffer);
    read(ctx->txp, buffer, [=](Error err) {
        ctx->logger->debug("ndt: wait close ... %d", (int)err);
        // Note: the server SHOULD close the connection
        if (err == EofError()) {
            ctx->logger->info("Connection closed");
            callback(NoError());
            return;
        }
        if (err == TimeoutError()) {
            ctx->logger->info("Closing connection after 1.0 sec timeout");
            callback(NoError());
            return;
        }
        if (err) {
            callback(err);
            return;
        }
        ctx->logger->debug("ndt: got extra data: %s", buffer->read().c_str());
        callback(GenericError());
    });
}

static inline void disconnect_and_callback_impl(Var<Context> ctx, Error err) {
    if (ctx->txp) {
        Var<Transport> txp = ctx->txp;
        ctx->txp = nullptr;
        txp->close([=]() { ctx->callback(err); });
        return;
    }
    ctx->callback(err);
}

} // namespace protocol
} // namespace mk
} // namespace ndt
#endif
