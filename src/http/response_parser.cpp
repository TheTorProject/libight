// Part of measurement-kit <https://measurement-kit.github.io/>.
// Measurement-kit is free software. See AUTHORS and LICENSE for more
// information on the copying conditions.

#include <functional>
#include <map>
#include <measurement_kit/common.hpp>
#include <measurement_kit/net.hpp>
#include <measurement_kit/http.hpp>
#include <stdexcept>
#include <string>
#include <stddef.h>
#include <string.h>
#include <type_traits>
#include "src/http/response_parser.hpp"
#include "ext/http-parser/http_parser.h"

#define MAXLINE 4096

namespace mk {
namespace http {

/*!
 * \brief Implementation of ResponseParser.
 * \see ResponseParser.
 */
class ResponseParserImpl {

    Logger *logger = Logger::global();
    http_parser parser;
    http_parser_settings settings;
    net::Buffer buffer;

    // Variables used during parsing
    std::string reason;
    unsigned int prev = S_NOTHING;
    std::string field;
    std::string value;
    Headers headers;
    bool closing = false;
    bool parsing = false;

    static int do_message_begin(http_parser *p) {
        auto impl = static_cast<ResponseParserImpl *>(p->data);
        impl->logger->debug("http: BEGIN");
        impl->reason = "";
        impl->prev = S_NOTHING;
        impl->field = "";
        impl->value = "";
        impl->headers.clear();
        impl->begin_fn();
        return 0;
    }

    static int do_status(http_parser *p, const char *s, size_t n) {
        auto impl = static_cast<ResponseParserImpl *>(p->data);
        impl->logger->debug("http: STATUS");
        impl->reason.append(s, n);
        return 0;
    }

    void do_header_internal(unsigned int cur, const char *s, size_t n) {
        //
        // This implements the finite state machine described by the
        // documentation of joyent/http-parser.
        //
        // See github.com/joyent/http-parser/blob/master/README.md#callbacks
        //
        if (prev == S_NOTHING && cur == S_FIELD) {
            field = std::string(s, n);
        } else if (prev == S_VALUE && cur == S_FIELD) {
            headers[std::move(field)] = std::move(value);
            field = std::string(s, n);
        } else if (prev == S_FIELD && cur == S_FIELD) {
            field.append(s, n);
        } else if (prev == S_FIELD && cur == S_VALUE) {
            value = std::string(s, n);
        } else if (prev == S_VALUE && cur == S_VALUE) {
            value.append(s, n);
        } else {
            throw std::runtime_error("Internal error");
        }
        prev = cur;
    }

    static int do_header_field(http_parser *p, const char *s, size_t n) {
        auto impl = static_cast<ResponseParserImpl *>(p->data);
        impl->logger->debug("http: FIELD");
        impl->do_header_internal(S_FIELD, s, n);
        return 0;
    }

    static int do_header_value(http_parser *p, const char *s, size_t n) {
        auto impl = static_cast<ResponseParserImpl *>(p->data);
        impl->logger->debug("http: VALUE");
        impl->do_header_internal(S_VALUE, s, n);
        return 0;
    }

    static int do_headers_complete(http_parser *p) {
        auto impl = static_cast<ResponseParserImpl *>(p->data);
        impl->logger->debug("http: HEADERS_COMPLETE");
        if (impl->field != "") { // Also copy last header
            impl->headers[std::move(impl->field)] = std::move(impl->value);
        }
        impl->headers_complete_fn(
            impl->parser.http_major, impl->parser.http_minor,
            impl->parser.status_code, std::move(impl->reason),
            std::move(impl->headers));
        return 0;
    }

    static int do_body(http_parser *p, const char *s, size_t n) {
        auto impl = static_cast<ResponseParserImpl *>(p->data);
        impl->logger->debug("http: BODY");
        //
        // By default the body handler is not set. This is to avoid
        // copying (s, n) into a string when you don't want to see
        // the response body, for efficiency.
        //
        if (impl->body_fn) {
            impl->body_fn(std::string(s, n));
        }
        return 0;
    }

    static int do_message_complete(http_parser *p) {
        auto impl = static_cast<ResponseParserImpl *>(p->data);
        impl->logger->debug("http: END");
        impl->end_fn();
        return 0;
    }

    void parse(void) {
        auto total = (size_t)0;
        buffer.for_each([&](const void *base, size_t count) {
            parsing = true;
            size_t n = http_parser_execute(&parser, &settings,
                                           (const char *)base, count);
            parsing = false;
            if (parser.upgrade) {
                throw UpgradeError();
            }
            if (n != count) {
                throw ParserError();
            }
            total += count;
            return true;
        });
        if (closing) {
            delete this;
            return;
        }
        buffer.discard(total);
    }

  public:
    /*!
     * \brief Default constructor.
     */
    ResponseParserImpl(Logger *lp = Logger::global()) : logger(lp) {
        http_parser_settings_init(&settings);
        settings.on_message_begin = do_message_begin;
        settings.on_status = do_status;
        settings.on_header_field = do_header_field;
        settings.on_header_value = do_header_value;
        settings.on_headers_complete = do_headers_complete;
        settings.on_body = do_body;
        settings.on_message_complete = do_message_complete;
        http_parser_init(&parser, HTTP_RESPONSE);
        parser.data = this; /* Which makes this object non-movable */
    }

    /*!
     * \brief Deleted copy constructor.
     */
    ResponseParserImpl(ResponseParserImpl &other) = delete;

    /*!
     * \brief Deleted copy assignment operator.
     */
    ResponseParserImpl &operator=(ResponseParserImpl &other) = delete;

    /*!
     * \brief Deleted move operator.
     */
    ResponseParserImpl(ResponseParserImpl &&other) = delete;

    /*!
     * \brief Deleted move assignment operator.
     */
    ResponseParserImpl &operator=(ResponseParserImpl &&other) = delete;

    /*!
     * \brief Handler for the `begin` event.
     */
    std::function<void(void)> begin_fn = [](void) {
        // nothing
    };

    /*!
     * \brief Handler for the `headers_complete` event.
     * \see RequestParser::on_headers_complete.
     */
    std::function<void(unsigned short, unsigned short, unsigned int,
                       std::string &&, Headers &&)> headers_complete_fn =
        [](unsigned short, unsigned short, unsigned int, std::string &&,
           Headers &&) {
            // nothing
        };

    /*!
     * \brief Handler for the `body` event.
     * \see RequestParser::on_body.
     */
    std::function<void(std::string &&)> body_fn;

    /*!
     * \brief Handler for the `end` event.
     */
    std::function<void(void)> end_fn = [](void) {
        // nothing
    };

    /*!
     * \brief Feed the parser.
     * \param data Evbuffer containing the received data.
     * \throws std::runtime_error This method throws std::runtime_error (or
     *         a class derived from it) on several error conditions.
     */
    void feed(net::Buffer &data) {
        buffer << data;
        parse();
    }

    /*!
     * \brief Feed the parser.
     * \param data String containing the received data.
     * \throws std::runtime_error This method throws std::runtime_error (or
     *         a class derived from it) on several error conditions.
     */
    void feed(std::string data) {
        buffer << data;
        parse();
    }

    /*!
     * \brief Feed the parser.
     * \param c Character containing the received data.
     * \remark This function is used for testing.
     * \throws std::runtime_error This method throws std::runtime_error (or
     *         a class derived from it) on several error conditions.
     */
    void feed(char c) {
        buffer.write((const void *)&c, 1);
        parse();
    }

    /*!
     * \brief Tell the parser we hit EOF.
     * \remark This allows us to implement the body-ends-at-EOF semantic.
     * \throws std::runtime_error This method throws std::runtime_error (or
     *         a class derived from it) on several error conditions.
     */
    void eof() {
        parsing = true;
        size_t n = http_parser_execute(&parser, &settings, NULL, 0);
        parsing = false;
        if (parser.upgrade) {
            throw UpgradeError();
        }
        if (n != 0) {
            throw ParserError();
        }
        if (closing) {
            delete this;
            return;
        }
    }

    /*!
     * \brief Destroy this parser.
     * \remark Actual destruction may be delayed if parser is currently
     *         parsing any incoming data.
     */
    void destroy() {
        if (closing) {
            return;
        }
        closing = true;
        if (parsing) {
            return;
        }
        delete this;
    }
};

//
// ResponseParser
//

ResponseParser::ResponseParser(Logger *lp) : impl(new ResponseParserImpl(lp)) {}

ResponseParser::~ResponseParser(void) {
    if (impl == nullptr) {
        return;
    }
    impl->destroy();
    impl = nullptr; // Idempotent
}

void ResponseParser::on_begin(std::function<void(void)> &&fn) {
    impl->begin_fn = std::move(fn);
}

void ResponseParser::on_headers_complete(
    std::function<void(unsigned short, unsigned short, unsigned int,
                       std::string &&, Headers &&)> &&fn) {
    impl->headers_complete_fn = std::move(fn);
}

void ResponseParser::on_body(std::function<void(std::string &&)> &&fn) {
    impl->body_fn = std::move(fn);
}

void ResponseParser::on_end(std::function<void(void)> &&fn) {
    impl->end_fn = std::move(fn);
}

void ResponseParser::feed(net::Buffer &data) { impl->feed(data); }

void ResponseParser::feed(std::string data) { impl->feed(data); }

void ResponseParser::feed(char c) { impl->feed(c); }

void ResponseParser::eof() { impl->eof(); }

} // namespace mk
} // namespace http
extern "C" {

using namespace mk::http;

static int cb_message_begin(http_parser *p) {
    return static_cast<ResponseParserNg *>(p->data)->do_message_begin_();
}

static int cb_status(http_parser *p, const char *s, size_t n) {
    return static_cast<ResponseParserNg *>(p->data)->do_status_(s, n);
}

static int cb_header_field(http_parser *p, const char *s, size_t n) {
    return static_cast<ResponseParserNg *>(p->data)->do_header_field_(s, n);
}

static int cb_header_value(http_parser *p, const char *s, size_t n) {
    return static_cast<ResponseParserNg *>(p->data)->do_header_value_(s, n);
}

static int cb_headers_complete(http_parser *p) {
    return static_cast<ResponseParserNg *>(p->data)->do_headers_complete_();
}

static int cb_body(http_parser *p, const char *s, size_t n) {
    return static_cast<ResponseParserNg *>(p->data)->do_body_(s, n);
}

static int cb_message_complete(http_parser *p) {
    return static_cast<ResponseParserNg *>(p->data)->do_message_complete_();
}

} // extern "C"
namespace mk {
namespace http {

ResponseParserNg::ResponseParserNg(Logger *logger) {
    logger_ = logger;
    http_parser_settings_init(&settings_);
    settings_.on_message_begin = cb_message_begin;
    settings_.on_status = cb_status;
    settings_.on_header_field = cb_header_field;
    settings_.on_header_value = cb_header_value;
    settings_.on_headers_complete = cb_headers_complete;
    settings_.on_body = cb_body;
    settings_.on_message_complete = cb_message_complete;
    http_parser_init(&parser_, HTTP_RESPONSE);
    parser_.data = this; /* Which makes this object non-movable */
}

} // namespace http
} // namespace mk
