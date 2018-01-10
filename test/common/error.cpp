// Part of Measurement Kit <https://measurement-kit.github.io/>.
// Measurement Kit is free software under the BSD license. See AUTHORS
// and LICENSE for more information on the copying conditions.

#define CATCH_CONFIG_MAIN
#include "src/libmeasurement_kit/ext/catch.hpp"

#include <measurement_kit/common.hpp>

using namespace mk;

TEST_CASE("The default constructed error is true-ish") {
    Error err;
    REQUIRE(!err);
    REQUIRE((err.child_errors.size() <= 0));
    REQUIRE(err == 0);
    REQUIRE(err.reason == "");
}

TEST_CASE("Error constructed with error code is correctly initialized") {
    Error err{17};
    REQUIRE(!!err);
    REQUIRE((err.child_errors.size() <= 0));
    REQUIRE(err == 17);
    REQUIRE(err.reason == "unknown_failure 17");
}

TEST_CASE("Error constructed with error and message is correctly initialized") {
    Error err{17, "antani"};
    REQUIRE(!!err);
    REQUIRE((err.child_errors.size() <= 0));
    REQUIRE(err == 17);
    REQUIRE(err.reason == "antani");
}

TEST_CASE("An error with underlying error works correctly") {
    Error err{17, "antani"};
    err.add_child_error(MockedError());
    REQUIRE(!!err);
    REQUIRE(err.child_errors[0] == MockedError());
    REQUIRE(err == 17);
    REQUIRE(err.reason == "antani");
}

TEST_CASE("Equality works for errors") {
    Error first{17}, second{17};
    REQUIRE(first == second);
}

TEST_CASE("Unequality works for errors") {
    Error first{17}, second{21};
    REQUIRE(first != second);
}

TEST_CASE("The defined-error constructor with string works") {
    Error ex = MockedError("antani");
    REQUIRE(!!ex);
    REQUIRE(ex.reason == "mocked_error: antani");
    REQUIRE(strcmp(ex.what(), "mocked_error: antani") == 0);
}

TEST_CASE("The add_child_error() method works") {
    Error err;
    err.add_child_error(MockedError("antani"));
    err.add_child_error(MockedError());
    REQUIRE((err.child_errors.size() == 2));
    REQUIRE((err.child_errors[0] == MockedError()));
    REQUIRE((err.child_errors[0].reason == "mocked_error: antani"));
    REQUIRE((err.child_errors[1] == MockedError()));
    REQUIRE((err.child_errors[1].reason == "mocked_error"));
}
