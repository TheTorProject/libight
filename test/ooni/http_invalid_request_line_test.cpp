// Part of measurement-kit <https://measurement-kit.github.io/>.
// Measurement-kit is free software. See AUTHORS and LICENSE for more
// information on the copying conditions.

#define CATCH_CONFIG_MAIN
#include <catch.hpp>

#include <chrono>
#include <iostream>
#include <list>
#include <measurement_kit/ooni.hpp>
#include <string>
#include <thread>
#include "src/ooni/ooni_test.hpp"

using namespace mk;

TEST_CASE("Synchronous http-invalid-request-line test") {
    Var<std::list<std::string>> logs(new std::list<std::string>);
    ooni::HttpInvalidRequestLineTest()
        .set_backend("http://213.138.109.232/")
        .on_log([=](const char *s) { logs->push_back(s); })
        .run();
    for (auto &s : *logs) std::cout << s << "\n";
}

TEST_CASE("Synchronous http-invalid-request-line test with HTTP backend") {
    Var<std::list<std::string>> logs(new std::list<std::string>);
    ooni::HttpInvalidRequestLineTest()
        .set_backend("http://data.neubot.org/") // Let's troll Davide!
        .on_log([=](const char *s) { logs->push_back(s); })
        .run();
    for (auto &s : *logs) std::cout << s << "\n";
}

TEST_CASE("Asynchronous http-invalid-request-line test") {
    Var<std::list<std::string>> logs(new std::list<std::string>);
    bool done = false;
    ooni::HttpInvalidRequestLineTest()
        .set_backend("http://213.138.109.232/")
        .on_log([=](const char *s) { logs->push_back(s); })
        .run([&done]() { done = true; });
    do {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    } while (!done);
    for (auto &s : *logs) std::cout << s << "\n";
}

TEST_CASE("Make sure that set_output_path() works") {
    auto instance = ooni::HttpInvalidRequestLineTest()
        .set_output_file_path("foo.txt")
        .create_test_();
    auto ptr = static_cast<ooni::OoniTest *>(instance.get());
    REQUIRE(ptr->get_report_filename() == "foo.txt");
}

TEST_CASE("Make sure that default get_output_path() is nonempty") {
    auto instance = ooni::HttpInvalidRequestLineTest()
        .create_test_();
    auto ptr = static_cast<ooni::OoniTest *>(instance.get());
    REQUIRE(ptr->get_report_filename() != "");
}
