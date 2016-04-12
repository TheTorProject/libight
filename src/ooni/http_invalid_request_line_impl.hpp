// Part of measurement-kit <https://measurement-kit.github.io/>.
// Measurement-kit is free software. See AUTHORS and LICENSE for more
// information on the copying conditions.

#ifndef SRC_OONI_HTTP_INVALID_REQUEST_LINE_HPP
#define SRC_OONI_HTTP_INVALID_REQUEST_LINE_HPP

#include "src/common/utils.hpp"
#include "src/ooni/errors.hpp"
#include "src/ooni/ooni_test_impl.hpp"
#include "src/ooni/http_test_impl.hpp"
#include <sys/stat.h>

using json = nlohmann::json;

namespace mk {
namespace ooni {

class HTTPInvalidRequestLineImpl : public HTTPTestImpl {
    using HTTPTestImpl::HTTPTestImpl;

    int tests_run = 0;


  public:
    HTTPInvalidRequestLineImpl(Settings options_) : HTTPTestImpl(options_) {
        test_name = "http_invalid_request_line";
        test_version = "0.0.1";
    };

    void main(Settings options, std::function<void(json)> &&cb) {

        auto handle_response = [this, cb](Error, http::Response &&) {
            tests_run += 1;
            if (tests_run == 3) {
                cb(entry);
            }
            // XXX we currently don't set the tampering key, because this test
            // speaks to a TCP Echo helper, hence the response will not be valid
            // HTTP.
        };

        http::Headers headers;
        // test_random_invalid_method
        // randomSTR(4) + " / HTTP/1.1\n\r"
        request(
            {
                {"url", options["backend"]},
                {"method", mk::random_str_uppercase(4)},
                {"http_version", "HTTP/1.1"},
            },
            headers, "", handle_response);

        // test_random_invalid_field_count
        // ' '.join(randomStr(5) for x in range(4)) + '\n\r'
        // XXX currently cannot be implemented using HTTP client lib.

        // test_random_big_request_method
        // randomStr(1024) + ' / HTTP/1.1\n\r'
        request(
            {
                {"url", options["backend"]},
                {"method", mk::random_str_uppercase(1024)},
                {"http_version", "HTTP/1.1"},
            },
            headers, "", handle_response);

        // test_random_invalid_version_number
        // 'GET / HTTP/' + randomStr(3)
        request(
            {
                {"url", options["backend"]},
                {"method", "GET"},
                {"http_version", "HTTP/" + mk::random_str(3)},
            },
            headers, "", handle_response);
    }
};

} // namespace ooni
} // namespace mk
#endif
