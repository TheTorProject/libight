// Part of measurement-kit <https://measurement-kit.github.io/>.
// Measurement-kit is free software. See AUTHORS and LICENSE for more
// information on the copying conditions.
#ifndef MEASUREMENT_KIT_OONI_TCP_CONNECT_TEST_HPP
#define MEASUREMENT_KIT_OONI_TCP_CONNECT_TEST_HPP

#include <measurement_kit/common.hpp>

namespace mk {
namespace ooni {

class TcpConnectTest : public NetTestDsl {
  public:
    Var<NetTest> create_test_() override;
};

} // namespace ooni
} // namespace mk
#endif
