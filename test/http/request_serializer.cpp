// Part of measurement-kit <https://measurement-kit.github.io/>.
// Measurement-kit is free software. See AUTHORS and LICENSE for more
// information on the copying conditions.

//
// Regression tests for `protocols/http.hpp` and `protocols/http.cpp`.
//

#define CATCH_CONFIG_MAIN
#include "src/ext/Catch/single_include/catch.hpp"

#include "src/http/request_serializer.hpp"
#include <measurement_kit/http.hpp>

using namespace mk;
using namespace mk::net;
using namespace mk::http;

TEST_CASE("HTTP Request serializer works as expected") {
    auto serializer = RequestSerializer(
        {
            {"http/follow_redirects", "yes"},
            {"http/url",
             "http://www.example.com/antani?clacsonato=yes#melandri"},
            {"http/ignore_body", "yes"},
            {"http/method", "GET"},
            {"http/http_version", "HTTP/1.0"},
        },
        {
            {"User-Agent", "Antani/1.0.0.0"},
        },
        "0123456789");
    Buffer buffer;
    serializer.serialize(buffer);
    auto serialized = buffer.read();
    std::string expect = "GET /antani?clacsonato=yes HTTP/1.0\r\n";
    expect += "User-Agent: Antani/1.0.0.0\r\n";
    expect += "Host: www.example.com\r\n";
    expect += "Content-Length: 10\r\n";
    expect += "\r\n";
    expect += "0123456789";
    REQUIRE(serialized == expect);
}

TEST_CASE("HTTP Request serializer works as expected with explicit path") {
    auto serializer = RequestSerializer(
        {
            {"http/follow_redirects", "yes"},
            {"http/url",
             "http://www.example.com/antani?clacsonato=yes#melandri"},
            {"http/path", "/antani?amicimiei"},
            {"http/ignore_body", "yes"},
            {"http/method", "GET"},
            {"http/http_version", "HTTP/1.0"},
        },
        {
            {"User-Agent", "Antani/1.0.0.0"},
        },
        "0123456789");
    Buffer buffer;
    serializer.serialize(buffer);
    auto serialized = buffer.read();
    std::string expect = "GET /antani?amicimiei HTTP/1.0\r\n";
    expect += "User-Agent: Antani/1.0.0.0\r\n";
    expect += "Host: www.example.com\r\n";
    expect += "Content-Length: 10\r\n";
    expect += "\r\n";
    expect += "0123456789";
    REQUIRE(serialized == expect);
}
