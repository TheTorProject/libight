// Part of measurement-kit <https://measurement-kit.github.io/>.
// Measurement-kit is free software. See AUTHORS and LICENSE for more
// information on the copying conditions.

#define CATCH_CONFIG_MAIN
#include "../src/libmeasurement_kit/ext/catch.hpp"

#include "../src/libmeasurement_kit/ooni/orchestrate_impl.hpp"
#include "../src/libmeasurement_kit/common/utils.hpp"

using namespace mk;
using namespace mk::ooni;
using namespace mk::ooni::orchestrate;

TEST_CASE("Authentication::load() works correctly") {
    const std::string fname = "orchestrator_dummy.json";
    Authentication auth;

    SECTION("with a nonexistent file") {
        REQUIRE(auth.load("/nonexistent") != NoError());
    }

    SECTION("with invalid JSON") {
        REQUIRE(overwrite_file(fname, "{") == NoError());
        REQUIRE(auth.load(fname) != NoError());
    }

    SECTION("with missing username field") {
        REQUIRE(overwrite_file(fname, "{}") == NoError());
        REQUIRE(auth.load(fname) != NoError());
    }

    SECTION("with missing password field") {
        REQUIRE(overwrite_file(fname, "{\"username\": \"xo\"}") == NoError());
        REQUIRE(auth.load(fname) != NoError());
    }

    SECTION("with good input") {
        nlohmann::json data{{"username", "xo"}, {"password", "xo"}};
        REQUIRE(overwrite_file(fname, data.dump()) == NoError());
        REQUIRE(auth.load(fname) == NoError());
        REQUIRE(auth.username == "xo");
        REQUIRE(auth.password == "xo");
    }
}

TEST_CASE("Authentication::store() works correctly") {
    const std::string fname = "orchestrator_dummy.json";
    Authentication auth;

    SECTION("with nonexistent file") {
        REQUIRE(auth.store("/nonexistent/nonexistent") != NoError());
    }

    SECTION("with existent file") {
        auth.username = auth.password = "xo";
        REQUIRE(auth.store(fname) == NoError());
        nlohmann::json data = nlohmann::json::parse(*slurp(fname));
        REQUIRE(data["username"] == "xo");
        REQUIRE(data["password"] == "xo");
    }
}

TEST_CASE("Authentication::is_valid() works correctly") {
    Authentication auth;

    SECTION("When we are not logged in") {
        REQUIRE(auth.is_valid() == false);
    }

    SECTION("When we are logged in and time is expired") {
        auth.expiry_time = std::time(nullptr) - 60; // One minute in the past
        auth.logged_in = true;
        REQUIRE(auth.is_valid() == false);
    }

    SECTION("When we are logged in and time is not expired") {
        auth.expiry_time = std::time(nullptr) + 60; // One minute in the future
        auth.logged_in = true;
        REQUIRE(auth.is_valid() == true);
    }
}

TEST_CASE("orchestrate::login() works correctly") {
    auto auth = Authentication::make();
    auto reactor = Reactor::make();

    SECTION("When the username is missing") {
        Error err;
        reactor->run_with_initial_event([&]() {
            login(auth, testing_registry_url(), {}, reactor, Logger::global(),
                  [&](Error &&e) {
                    err = e;
                    reactor->break_loop();
                  });
        });
        REQUIRE(err == MissingRequiredValueError());
    }

    SECTION("When the password is missing") {
        auth->username = "antani";
        Error err;
        reactor->run_with_initial_event([&]() {
            login(auth, testing_registry_url(), {}, reactor, Logger::global(),
                  [&](Error &&e) {
                    err = e;
                    reactor->break_loop();
                  });
        });
        REQUIRE(err == MissingRequiredValueError());
    }

    /*
     * TODO: add more tests
     */
}

#ifdef ENABLE_INTEGRATION_TESTS

TEST_CASE("Orchestration works") {
    auto make_path = []() {
        std::string s = "orchestrator_secrets_";
        s += random_str(8);
        s += ".json";
        return s;
    };
    Client client;
    client.logger->set_verbosity(MK_LOG_DEBUG2);
    client.probe_cc = "IT";
    client.probe_asn = "AS0";
    client.platform = "macos";
    client.supported_tests = {"web_connectivity"};
    client.network_type = "wifi";
    client.available_bandwidth = "10";
    //client.device_token = "{TOKEN}";  /* Not needed on PC devices */
    client.registry_url = testing_registry_url();
    client.secrets_path = make_path();
    std::promise<Error> promise;
    std::future<Error> future = promise.get_future();
    client.register_probe([client, &promise](Error &&error) {
        if (error) {
            promise.set_value(error);
            return;
        }
        client.update([&promise](Error &&error) {
            promise.set_value(error);
        });
    });
    REQUIRE(future.get() == NoError());
}

#endif
