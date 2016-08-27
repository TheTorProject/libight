// Part of measurement-kit <https://measurement-kit.github.io/>.
// Measurement-kit is free software. See AUTHORS and LICENSE for more
// information on the copying conditions.

#ifdef ENABLE_INTEGRATION_TESTS

#define CATCH_CONFIG_MAIN

#include "../src/libmeasurement_kit/ext/Catch/single_include/catch.hpp"
#include <measurement_kit/ndt.hpp>

using namespace mk::ndt;
using namespace mk;

TEST_CASE("The NDT test DSL works") {
    NdtTest().set_verbosity(MK_LOG_INFO).run();
}

TEST_CASE("Running the NDT test using begin / end works") {
    // XXX forced to use another reactor because the default one is already
    // used by the runner above and hence is busy!
    Var<Reactor> reactor = Reactor::make();
    Var<NetTest> test{new NdtTest};
    test->reactor = reactor;
    test->set_verbosity(MK_LOG_INFO);
    reactor->loop_with_initial_event([&]() {
        // TODO: do not ignore errors here, perhaps
        test->begin([=](Error) {
            test->end([=](Error) {
                reactor->break_loop();
            });
        });
    });
}

#else
int main(){}
#endif
