// Part of measurement-kit <https://measurement-kit.github.io/>.
// Measurement-kit is free software. See AUTHORS and LICENSE for more
// information on the copying conditions.
#ifndef SRC_HTTP_REQUEST_HPP
#define SRC_HTTP_REQUEST_HPP

#include <functional>
#include <map>
#include <measurement_kit/common.hpp>
#include <measurement_kit/net.hpp>
#include <measurement_kit/http.hpp>
#include <memory>
#include <set>
#include <string>
#include <type_traits>
#include "src/http/request_serializer.hpp"
#include "src/http/response_parser.hpp"
#include "src/http/stream.hpp"

namespace mk {
namespace http {

class Request : public NonCopyable, public NonMovable {

    RequestCallback callback;
    RequestSerializer serializer;
    Var<Stream> stream;
    Response response;
    std::set<Request *> *parent = nullptr;
    Logger *logger = Logger::global();
    Poller *poller = Poller::global();

    void emit_end(Error error, Response &&response) {
        close();
        callback(error, std::move(response));
        //
        // Self cleanup when we're owned by a Client.
        //
        // Note: we only detach ourself if we reach the final state, otherwise
        // the Client will manage to detach this object.
        //
        if (parent != nullptr) {
            parent->erase(this);
            delete this;
            return;
        }
    }

  public:
    /*!
     * \brief Constructor.
     * \param settings_ A std::map with key values of the options supported:
     *                     {
     *                         "follow_redirects": "yes|no",
     *                         "url": std::string,
     *                         "ignore_body": "yes|no",
     *                         "method": "GET|DELETE|PUT|POST|HEAD|...",
     *                         "http_version": "HTTP/1.1",
     *                         "path": by default is taken from the url
     *                     }
     * \param headers Request headers.
     * \param callback Function invoked when request is complete.
     * \param logger Logger to be used.
     * \param pol Poller to be used.
     * \param parent Pointer to parent to implement self clean up.
     */
    Request(const Settings settings_, Headers headers, std::string body,
            RequestCallback &&callback_, Logger *lp = Logger::global(),
            Poller *pol = Poller::global(),
            std::set<Request *> *parent_ = nullptr)
        : callback(callback_), parent(parent_), logger(lp), poller(pol) {
        auto settings = settings_; // Make a copy and work on that
        try {
            serializer = RequestSerializer(settings, headers, body);
        } catch (std::exception &) {
            callback(GenericError(), response);
            return;
        }
        // Extend settings with address and port to connect to
        settings["port"] = std::to_string(serializer.url.port);
        settings["address"] = serializer.url.address;

        // If needed, extend settings with socks5 proxy info
        if (serializer.url.schema == "httpo") {
            // tor_socks_port takes precedence because it's more specific
            if (settings.find("tor_socks_port") != settings.end()) {
                std::string proxy = "127.0.0.1:";
                proxy += settings["tor_socks_port"];
                settings["socks5_proxy"] = proxy;
            } else if (settings.find("socks5_proxy") == settings.end()) {
                settings["socks5_proxy"] = "127.0.0.1:9050";
            }
        }

        stream = std::make_shared<Stream>(settings, logger, poller);
        stream->on_error([this](Error err) {
            if (err != net::EofError()) {
                emit_end(err, std::move(response));
            } else {
                // When EOF is received, on_end() is called, therefore we
                // don't need to call emit_end() again here.
            }
        });
        stream->on_connect([this]() {
            // TODO: improve the way in which we serialize the request
            //       to reduce unnecessary copies
            net::Buffer buf;
            serializer.serialize(buf);
            *stream << buf.read();

            stream->on_flush([this]() {
                logger->debug("http: request sent... waiting for response");
            });

            stream->on_headers_complete([&](
                unsigned short major, unsigned short minor, unsigned int status,
                std::string &&reason, Headers &&headers) {
                logger->debug("http: headers received...");
                response.http_major = major;
                response.http_minor = minor;
                response.status_code = status;
                response.reason = std::move(reason);
                response.headers = std::move(headers);
            });

            stream->on_body([&](std::string &&chunk) {
                logger->debug("http: received body chunk...");
                // FIXME: I am not sure whether the body callback
                //        is still needed or not...
                response.body += chunk;
            });

            stream->on_end([&]() {
                logger->debug("http: we have reached end of response");
                emit_end(NoError(), std::move(response));
            });

        });
    }

    Var<Stream> get_stream() { return stream; }

    void close() {
        if (stream) stream->close();
    }

    ~Request() { close(); }

    std::string socks5_address() { return stream->socks5_address(); }

    std::string socks5_port() { return stream->socks5_port(); }
};

typedef std::function<void(Error, Var<Transport>)> RequestConnectCb;
typedef std::function<void(Error)> RequestSendCb;
typedef std::function<void(Error, Var<Response>)> RequestRecvResponseCb;
typedef std::function<void(Error, Var<Response>)> RequestSendrecvCb;

template<void (*do_connect)(std::string, int,
        std::function<void(Error, Var<Transport>)>,
        Settings, Logger *, Poller *) = net::connect>
void request_connect(Settings settings, RequestConnectCb cb,
        Poller *poller = Poller::global(), Logger *logger = Logger::global()) {
    if (settings.find("url") == settings.end()) {
        cb(MissingUrlError(), nullptr);
        return;
    }
    ErrorOr<Url> url = parse_url_noexcept(settings.at("url"));
    if (!url) {
        cb(url.as_error(), nullptr);
        return;
    }
    if (url->schema == "httpo") {
        // tor_socks_port takes precedence because it's more specific
        if (settings.find("tor_socks_port") != settings.end()) {
            std::string proxy = "127.0.0.1:";
            proxy += settings["tor_socks_port"];
            settings["socks5_proxy"] = proxy;
        } else if (settings.find("socks5_proxy") == settings.end()) {
            settings["socks5_proxy"] = "127.0.0.1:9050";
        }
    }
    do_connect(url->address, url->port, cb, settings, logger, poller);
}

void request_send(Var<Transport>, Settings, Headers, std::string,
        RequestSendCb);

void request_recv_response(Var<Transport>, RequestRecvResponseCb,
        Poller * = Poller::global(), Logger * = Logger::global());

void request_sendrecv(Var<Transport>, Settings, Headers, std::string,
        RequestSendrecvCb, Poller * = Poller::global(),
        Logger * = Logger::global());

void request_cycle(Settings, Headers, std::string, RequestSendrecvCb,
        Poller * = Poller::global(), Logger * = Logger::global());

} // namespace http
} // namespace mk
#endif
