// Part of measurement-kit <https://measurement-kit.github.io/>.
// Measurement-kit is free software. See AUTHORS and LICENSE for more
// information on the copying conditions.
#ifndef SRC_HTTP_RESPONSE_PARSER_HPP
#define SRC_HTTP_RESPONSE_PARSER_HPP

#include "ext/http-parser/http_parser.h"
#include <measurement_kit/common.hpp>
#include <measurement_kit/net.hpp>
#include <measurement_kit/http.hpp>

#include <functional>
#include <iosfwd>
#include <map>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace mk {
namespace http {
using namespace mk::net;

// TODO: it would probably optimal to merge `ResponseParserImpl` with this
// class because actually here we have just code duplication after refactoring
// that simplified the logic of `ResponseParserImpl`.

class ResponseParserImpl; // Forward declaration

class ResponseParser : public NonCopyable, public NonMovable {
  public:
    ResponseParser(Logger * = Logger::global());
    ~ResponseParser();

    void on_begin(std::function<void()> &&fn);

    void on_headers_complete(
        std::function<void(unsigned short http_major, unsigned short http_minor,
                           unsigned int status_code, std::string &&reason,
                           Headers &&headers)> &&fn);

    void on_body(std::function<void(std::string &&)> &&fn);

    void on_end(std::function<void()> &&fn);

    void feed(Buffer &data);

    void feed(std::string data);

    void feed(char c);

    void eof();

  protected:
    ResponseParserImpl *impl = nullptr;
};

// Header parsing states (see do_header_internal) - used by old impl.
#define S_NOTHING 0
#define S_FIELD 1
#define S_VALUE 2

enum class HeaderParserState {
    NOTHING = 0,
    FIELD = 1,
    VALUE = 2,
};

class ResponseParserNg : public NonCopyable, public NonMovable {
  public:
    ResponseParserNg(Logger * = Logger::global());

    void on_begin(std::function<void()> fn) { begin_fn_ = fn; }

    void on_response(std::function<void(Response)> fn) { response_fn_ = fn; }

    void on_body(std::function<void(std::string)> fn) { body_fn_ = fn; }

    void on_end(std::function<void()> fn) { end_fn_ = fn; }

    void feed(Buffer &data) {
        buffer_ << data;
        parse();
    }

    void feed(std::string data) {
        buffer_ << data;
        parse();
    }

    void feed(const char c) {
        buffer_.write((const void *)&c, 1);
        parse();
    }

    void eof() { parser_execute(nullptr, 0); }

    int do_message_begin_() {
        logger_->debug("http: BEGIN");
        response_ = Response();
        prev_ = HeaderParserState::NOTHING;
        field_ = "";
        value_ = "";
        if (begin_fn_) {
            begin_fn_();
        }
        return 0;
    }

    int do_status_(const char *s, size_t n) {
        logger_->debug("http: STATUS");
        response_.reason.append(s, n);
        return 0;
    }

    int do_header_field_(const char *s, size_t n) {
        logger_->debug("http: FIELD");
        do_header_internal(HeaderParserState::FIELD, s, n);
        return 0;
    }

    int do_header_value_(const char *s, size_t n) {
        logger_->debug("http: VALUE");
        do_header_internal(HeaderParserState::VALUE, s, n);
        return 0;
    }

    int do_headers_complete_() {
        logger_->debug("http: HEADERS_COMPLETE");
        if (field_ != "") { // Also copy last header
            response_.headers[field_] = value_;
        }
        response_.http_major = parser_.http_major;
        response_.status_code = parser_.status_code;
        response_.http_minor = parser_.http_minor;
        if (response_fn_) {
            response_fn_(response_);
        }
        return 0;
    }

    int do_body_(const char *s, size_t n) {
        logger_->debug("http: BODY");
        if (body_fn_) {
            body_fn_(std::string(s, n));
        }
        return 0;
    }

    int do_message_complete_() {
        logger_->debug("http: END");
        if (end_fn_) {
            end_fn_();
        }
        return 0;
    }

  private:
    SafelyOverridableFunc<void()> begin_fn_;
    SafelyOverridableFunc<void(Response)> response_fn_;
    SafelyOverridableFunc<void(std::string)> body_fn_;
    SafelyOverridableFunc<void()> end_fn_;

    Logger *logger_ = Logger::global();
    http_parser parser_;
    http_parser_settings settings_;
    Buffer buffer_;

    // Variables used during parsing
    Response response_;
    HeaderParserState prev_ = HeaderParserState::NOTHING;
    std::string field_;
    std::string value_;

    void do_header_internal(HeaderParserState cur, const char *s, size_t n) {
        using HPS = HeaderParserState;
        //
        // This implements the finite state machine described by the
        // documentation of joyent/http-parser.
        //
        // See github.com/joyent/http-parser/blob/master/README.md#callbacks
        //
        if (prev_ == HPS::NOTHING && cur == HPS::FIELD) {
            field_ = std::string(s, n);
        } else if (prev_ == HPS::VALUE && cur == HPS::FIELD) {
            response_.headers[field_] = value_;
            field_ = std::string(s, n);
        } else if (prev_ == HPS::FIELD && cur == HPS::FIELD) {
            field_.append(s, n);
        } else if (prev_ == HPS::FIELD && cur == HPS::VALUE) {
            value_ = std::string(s, n);
        } else if (prev_ == HPS::VALUE && cur == HPS::VALUE) {
            value_.append(s, n);
        } else {
            throw GenericError();
        }
        prev_ = cur;
    }

    void parse() {
        size_t total = 0;
        buffer_.for_each([&](const void *p, size_t n) {
            total += parser_execute(p, n);
            return true;
        });
        buffer_.discard(total);
    }

    size_t parser_execute(const void *p, size_t n) {
        size_t x =
            http_parser_execute(&parser_, &settings_, (const char *)p, n);
        if (parser_.upgrade) {
            throw UpgradeError();
        }
        if (x != n) {
            throw ParserError();
        }
        return n;
    }
};

} // namespace http
} // namespace mk
#endif
