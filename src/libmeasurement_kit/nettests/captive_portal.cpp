// Part of measurement-kit <https://measurement-kit.github.io/>.
// Measurement-kit is free software. See AUTHORS and LICENSE for more
// information on the copying conditions.

#include <measurement_kit/nettests.hpp>
#include <measurement_kit/ooni.hpp>

namespace mk {
namespace nettests {

CaptivePortalTest::CaptivePortalTest() : BaseTest() {
    runnable.reset(new CaptivePortalRunnable);
    runnable->test_name = "captive_portal";
    runnable->test_version = "0.0.1";
}

void CaptivePortalRunnable::main(std::string input, Settings options,
                                   Callback<Var<report::Entry>> cb) {
    ooni::captive_portal(input, options, cb, reactor, logger);
}

} // namespace nettests
} // namespace mk
