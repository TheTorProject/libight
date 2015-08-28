// Part of measurement-kit <https://measurement-kit.github.io/>.
// Measurement-kit is free software. See AUTHORS and LICENSE for more
// information on the copying conditions.

#ifndef MEASUREMENT_KIT_COMMON_ASYNC_HPP
#define MEASUREMENT_KIT_COMMON_ASYNC_HPP

#include <measurement_kit/common/net_test.hpp>
#include <measurement_kit/common/pointer.hpp>

namespace measurement_kit {
namespace common {

struct AsyncState;

class Async {
  public:

    /// Default constructor
    Async();

    /// Run the specified network test and call a callback when done
    /// \param func Callback called when test is done
    void run_test(SharedPointer<NetTest> test,
      std::function<void(SharedPointer<NetTest>)> func);

    /// Break out of the loop
    void break_loop();

    /// Restart the background loop
    void restart_loop();

    /// Returns true when no async jobs are running
    bool empty();

    /// Emit test complete events in the current thread
    void pump();

  private:
    SharedPointer<AsyncState> state;
    static void loop_thread(SharedPointer<AsyncState>);
};

} // namespace common
} // namespace measurement_kit
#endif
