// Part of measurement-kit <https://measurement-kit.github.io/>.
// Measurement-kit is free software. See AUTHORS and LICENSE for more
// information on the copying conditions.

#define CATCH_CONFIG_MAIN
#include "../src/libmeasurement_kit/ext/catch.hpp"

#include "../src/libmeasurement_kit/ooni/bouncer_impl.hpp"

using namespace mk;

static nlohmann::json do_out_of_range(std::string) {
    throw std::out_of_range("out of range");
}

static nlohmann::json do_domain_error(std::string) {
    throw std::domain_error("domain error");
}

TEST_CASE("BouncerReply::create() works") {

    SECTION("When the collector is not found") {
        auto maybe_reply = ooni::BouncerReply::create(
            R"({"error": "collector-not-found"})", Logger::global());
        REQUIRE(!maybe_reply);
        REQUIRE(maybe_reply.as_error() ==
                ooni::BouncerCollectorNotFoundError());
    }

    SECTION("When the response is invalid") {
        auto maybe_reply = ooni::BouncerReply::create(
            R"({"error": "invalid-request"})", Logger::global());
        REQUIRE(!maybe_reply);
        REQUIRE(maybe_reply.as_error() == ooni::BouncerInvalidRequestError());
    }

    SECTION("When the error is something else") {
        auto maybe_reply = ooni::BouncerReply::create(
            R"({"error": "xx"})", Logger::global());
        REQUIRE(!maybe_reply);
        REQUIRE(maybe_reply.as_error() == ooni::BouncerGenericError());
    }

    SECTION("When the net-tests section is missing") {
        auto maybe_reply = ooni::BouncerReply::create(
            R"({})", Logger::global());
        REQUIRE(!maybe_reply);
        REQUIRE(maybe_reply.as_error() ==
                ooni::BouncerTestHelperNotFoundError());
    }

    SECTION("When the parser throws invalid_argument") {
        auto maybe_reply = ooni::BouncerReply::create(
            R"({)", Logger::global());
        REQUIRE(!maybe_reply);
        REQUIRE(maybe_reply.as_error() == JsonParseError());
    }

    SECTION("When the parser throws out_of_range") {
        auto maybe_reply = ooni::bouncer::create_impl<do_out_of_range>(
            R"({})", Logger::global());
        REQUIRE(!maybe_reply);
        REQUIRE(maybe_reply.as_error() == JsonKeyError());
    }

    SECTION("When the parser throws domain_error") {
        auto maybe_reply = ooni::bouncer::create_impl<do_domain_error>(
            R"({})", Logger::global());
        REQUIRE(!maybe_reply);
        REQUIRE(maybe_reply.as_error() == JsonDomainError());
    }
}

TEST_CASE("BouncerReply accessors are robust to missing fields") {
    auto maybe_reply = ooni::BouncerReply::create(
        R"({"net-tests": [{"test-helpers-alternate":[], "collector-alternate":1234}]})",
        Logger::global());
    REQUIRE(!!maybe_reply);
    auto reply = *maybe_reply;

    SECTION("For get_collector") {
        auto maybe_value = reply->get_collector();
        REQUIRE(!maybe_value);
        REQUIRE(maybe_value.as_error() == ooni::BouncerValueNotFoundError());
    }

    SECTION("For get_collector_alternate") {
        auto maybe_value = reply->get_collector_alternate("xx");
        REQUIRE(!maybe_value);
        REQUIRE(maybe_value.as_error() == ooni::BouncerValueNotFoundError());
    }

    SECTION("For get_name") {
        auto maybe_value = reply->get_name();
        REQUIRE(!maybe_value);
        REQUIRE(maybe_value.as_error() == ooni::BouncerValueNotFoundError());
    }

    SECTION("For get_test_helper") {
        auto maybe_value = reply->get_test_helper("xx");
        REQUIRE(!maybe_value);
        REQUIRE(maybe_value.as_error() == ooni::BouncerValueNotFoundError());
    }

    SECTION("For get_test_helper_alternate") {
        auto maybe_value = reply->get_test_helper_alternate("xx", "yy");
        REQUIRE(!maybe_value);
        REQUIRE(maybe_value.as_error() == ooni::BouncerValueNotFoundError());
    }

    SECTION("For get_version") {
        auto maybe_value = reply->get_version();
        REQUIRE(!maybe_value);
        REQUIRE(maybe_value.as_error() == ooni::BouncerValueNotFoundError());
    }
}

static void request_error(Settings, http::Headers, std::string,
                          Callback<Error, Var<http::Response>> cb,
                          Var<Reactor> = Reactor::global(),
                          Var<Logger> = Logger::global(),
                          Var<http::Response> = nullptr, int = 0) {
    cb(MockedError(), nullptr);
}

TEST_CASE("post_net_tests() works") {

    SECTION("On network error") {
        Var<Reactor> reactor = Reactor::make();
        reactor->loop_with_initial_event([=]() {
            // Mocked http request that returns an invalid-request
            ooni::bouncer::post_net_tests_impl<request_error>(
                "https://a.collector.ooni.io/bouncer", "web-connectivity",
                "0.0.1", {"web-connectivity"},
                [=](Error e, Var<ooni::BouncerReply>) {
                    REQUIRE(e == MockedError());
                    reactor->break_loop();
                },
                {}, reactor, Logger::global());
        });
    }

#ifdef ENABLE_INTEGRATION_TESTS

    SECTION("When the collector is not found") {
        Var<Reactor> reactor = Reactor::make();
        reactor->loop_with_initial_event([=]() {
            ooni::bouncer::post_net_tests(
                "https://a.collector.ooni.io/bouncer", "antani", "0.0.1",
                {"antani"},
                [=](Error e, Var<ooni::BouncerReply>) {
                    REQUIRE(e == ooni::BouncerCollectorNotFoundError());
                    reactor->break_loop();
                },
                {}, reactor, Logger::global());
        });
    }

    SECTION("When the input is correct") {
        Var<Reactor> reactor = Reactor::make();
        reactor->loop_with_initial_event([=]() {
            ooni::bouncer::post_net_tests(
                "https://a.collector.ooni.io/bouncer", "web-connectivity",
                "0.0.1", {"web-connectivity"},
                [=](Error e, Var<ooni::BouncerReply> reply) {
                    REQUIRE(!e);
                    REQUIRE(*reply->get_collector() ==
                            "httpo://ihiderha53f36lsd.onion");
                    REQUIRE(*reply->get_collector_alternate("https") ==
                            "https://a.collector.ooni.io:4441");
                    REQUIRE(*reply->get_collector_alternate("cloudfront") ==
                            "https://das0y2z2ribx3.cloudfront.net");
                    REQUIRE(*reply->get_name() == "web-connectivity");
                    REQUIRE(*reply->get_test_helper("web-connectivity") ==
                            "httpo://7jne2rpg5lsaqs6b.onion");
                    REQUIRE(*reply->get_test_helper_alternate(
                                "web-connectivity", "https") ==
                            "https://a.web-connectivity.th.ooni.io:4442");
                    REQUIRE(*reply->get_test_helper_alternate(
                                "web-connectivity", "cloudfront") ==
                            "https://d2vt18apel48hw.cloudfront.net");
                    reactor->break_loop();
                },
                {}, reactor, Logger::global());
        });
    }
#endif
}
