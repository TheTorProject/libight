// Part of measurement-kit <https://measurement-kit.github.io/>.
// Measurement-kit is free software. See AUTHORS and LICENSE for more
// information on the copying conditions.

#define CATCH_CONFIG_MAIN
#include "src/ext/Catch/single_include/catch.hpp"

#include <measurement_kit/common.hpp>
#include "src/common/check_connectivity.hpp"
#include "src/common/utils.hpp"
#include "src/common/libs_impl.hpp"

#include <event2/dns.h>

#include <functional>
#include <new>
#include <stdexcept>

using namespace mk;

TEST_CASE("Constructor") {

    SECTION("We deal with event_base_new() failure") {
        auto libs = Libs();

        libs.event_base_new = [](void) { return ((event_base *)nullptr); };

        auto bad_alloc_fired = false;
        try {
            Poller poller(&libs);
        } catch (std::bad_alloc &) {
            bad_alloc_fired = true;
        }

        REQUIRE(bad_alloc_fired);
    }
}

TEST_CASE("The destructor works properly") {

    auto libs = Libs();

    auto event_base_free_fired = false;

    libs.event_base_free = [&event_base_free_fired](event_base *b) {
        event_base_free_fired = true;
        ::event_base_free(b);
    };

    { Poller poller(&libs); }

    REQUIRE(event_base_free_fired);
}

TEST_CASE("poller.loop() works properly in corner cases") {

    SECTION("We deal with event_base_dispatch() returning -1") {
        Libs libs;

        libs.event_base_dispatch = [](event_base *) { return (-1); };

        Poller poller1(&libs);

        auto runtime_error_fired = false;
        try {
            poller1.loop();
        } catch (std::runtime_error &) {
            runtime_error_fired = true;
        }

        REQUIRE(runtime_error_fired);
    }

    SECTION("We deal with event_base_dispatch() returning 1") {
        Libs libs;

        libs.event_base_dispatch = [](event_base *) { return (1); };
        Poller poller2(&libs);
        poller2.loop();
    }
}

TEST_CASE("poller.break_loop() works properly") {
    Libs libs;

    libs.event_base_loopbreak = [](event_base *) { return (-1); };

    Poller poller(&libs);

    auto runtime_error_fired = false;
    try {
        poller.break_loop();
    } catch (std::runtime_error &) {
        runtime_error_fired = true;
    }

    REQUIRE(runtime_error_fired);
}

TEST_CASE("poller.call_soon() works") {
    mk::Poller poller;
    auto now = mk::time_now();
    poller.call_soon([&poller]() { poller.break_loop(); });
    poller.loop();
    REQUIRE((mk::time_now() - now) < 1.00); // Very conservative check
}

TEST_CASE("poller.call_later() works") {
    mk::Poller poller;
    auto now = mk::time_now();
    poller.call_later(3.14, [&poller]() { poller.break_loop(); });
    poller.loop();
    REQUIRE((mk::time_now() - now) > 3.00); // Very conservative check
}

TEST_CASE("The periodic event is fired when we call loop()") {
    mk::Poller poller;
    unsigned int count = 0;
    poller.on_periodic_([&count](Poller *poller) {
        if (++count < 3) {
            return;
        }
        poller->break_loop();
    });
    poller.loop();
    REQUIRE(count == 3);
}
