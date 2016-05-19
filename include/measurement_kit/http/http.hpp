// Part of measurement-kit <https://measurement-kit.github.io/>.
// Measurement-kit is free software. See AUTHORS and LICENSE for more
// information on the copying conditions.
#ifndef MEASUREMENT_KIT_HTTP_HTTP_HPP
#define MEASUREMENT_KIT_HTTP_HTTP_HPP

#include <functional>
#include <map>
#include <measurement_kit/common.hpp>
#include <measurement_kit/net.hpp>
#include <set>
#include <string>

namespace mk {
namespace http {

MK_DEFINE_ERR(3000, UpgradeError, "")
MK_DEFINE_ERR(3001, ParserError, "")
MK_DEFINE_ERR(3002, UrlParserError, "")
MK_DEFINE_ERR(3003, MissingUrlSchemaError, "")
MK_DEFINE_ERR(3004, MissingUrlHostError, "")
MK_DEFINE_ERR(3005, MissingUrlError, "")

/// HTTP headers.
typedef std::map<std::string, std::string> Headers;

/// HTTP response.
struct Response {
    std::string response_line; ///< Original HTTP response line.
    unsigned short http_major; ///< HTTP major version number.
    unsigned short http_minor; ///< HTTP minor version number.
    unsigned int status_code;  ///< HTTP status code.
    std::string reason;        ///< HTTP reason string.
    Headers headers;           ///< Response headers.
    std::string body;          ///< Response body.
};

/// Type of callback called on error or when response is complete.
typedef Callback<Error, Response> RequestCallback;

// Forward declaration of internally used class.
class Request;

/// Send HTTP request and receive response.
/// \param settings Settings for HTTP request.
/// \param cb Callback called when complete or on error.
/// \param headers Optional HTTP request headers.
/// \param body Optional HTTP request body.
/// \param lp Optional logger.
/// \param pol Optional poller.
void request(Settings settings, RequestCallback cb, Headers headers = {},
             std::string body = "", Var<Logger> lp = Logger::global(),
             Var<Reactor> reactor = Reactor::global());

// Signature of the old http::Client ->request method, widely used
inline void request(Settings settings, Headers headers, std::string body,
        RequestCallback cb, Var<Logger> lp = Logger::global(),
        Var<Reactor> reactor = Reactor::global()) {
    request(settings, cb, headers, body, lp, reactor);
}

/// Represents a URL.
class Url {
  public:
    std::string schema;    /// URL schema
    std::string address;   /// URL address
    int port = 80;         /// URL port
    std::string path;      /// URL path
    std::string query;     /// URL query
    std::string pathquery; /// URL path followed by optional query
};

/// Parses a URL.
/// \param url Input URL you want to parse.
/// \return The parsed URL.
/// \throw Exception on failure.
Url parse_url(std::string url);

/// Parses a URL without throwing an exception on failure.
/// \param url Input URL you want to parse..
/// \return An error (on failure) or the parsed URL.
ErrorOr<Url> parse_url_noexcept(std::string url);

/// Send HTTP GET and receive response.
/// \param url URL to send request to.
/// \param settings Settings for HTTP request.
/// \param cb Callback called when complete or on error.
/// \param headers Optional HTTP request headers.
/// \param body Optional HTTP request body.
/// \param lp Optional logger.
/// \param pol Optional poller.
inline void get(std::string url, RequestCallback cb,
                Headers headers = {}, std::string body = "",
                Settings settings = {}, Var<Logger> lp = Logger::global(),
                Var<Reactor> reactor = Reactor::global()) {
    settings["http/method"] = "GET";
    settings["http/url"] = url;
    request(settings, cb, headers, body, lp, reactor);
}

/// Send HTTP request and receive response.
/// \param method Method to use.
/// \param url URL to send request to.
/// \param settings Settings for HTTP request.
/// \param cb Callback called when complete or on error.
/// \param headers Optional HTTP request headers.
/// \param body Optional HTTP request body.
/// \param lp Optional logger.
/// \param pol Optional poller.
inline void request(std::string method, std::string url, RequestCallback cb,
                    Headers headers = {}, std::string body = "",
                    Settings settings = {}, Var<Logger> lp = Logger::global(),
                    Var<Reactor> reactor = Reactor::global()) {
    settings["http/method"] = method;
    settings["http/url"] = url;
    request(settings, cb, headers, body, lp, reactor);
}

typedef std::function<void(Error)> RequestSendCb;

void request_connect(Settings, Callback<Error, Var<net::Transport>>,
                     Var<Reactor> = Reactor::global(),
                     Var<Logger> = Logger::global());

void request_send(Var<net::Transport>, Settings, Headers, std::string,
        RequestSendCb);

void request_recv_response(Var<net::Transport>, Callback<Error, Var<Response>>,
        Var<Reactor> = Reactor::global(), Var<Logger> = Logger::global());

void request_sendrecv(Var<net::Transport>, Settings, Headers, std::string,
        Callback<Error, Var<Response>>, Var<Reactor> = Reactor::global(),
        Var<Logger> = Logger::global());

void request_cycle(Settings, Headers, std::string, Callback<Error, Var<Response>>,
        Var<Reactor> = Reactor::global(), Var<Logger> = Logger::global());

} // namespace http
} // namespace mk
#endif
