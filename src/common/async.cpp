// Part of measurement-kit <https://measurement-kit.github.io/>.
// Measurement-kit is free software. See AUTHORS and LICENSE for more
// information on the copying conditions.

#include <measurement_kit/common/async.hpp>
#include <measurement_kit/common/logger.hpp>
#include <measurement_kit/common/net_test.hpp>
#include <measurement_kit/common/reactor.hpp>

namespace mk {

Async::Async() {}

void Async::run_test(Var<NetTest> test, std::function<void(Var<NetTest>)> fn) {
    if (!running) {
        thread = std::thread([this]() { reactor->loop(); });
        running = true;
    }
    active += 1;
    debug("async: scheduling %llu", test->identifier());
    reactor->call_later(1.0, [=]() {
        debug("async: starting %llu", test->identifier());
        test->begin([=]() {
            debug("async: ending %llu", test->identifier());
            test->end([=]() {
                debug("async: cleaning-up %llu", test->identifier());
                reactor->call_soon([=]() {
                    debug("async: callbacking %llu", test->identifier());
                    active -= 1;
                    debug("async: #active tasks: %d", (int)active);
                    fn(test);
                });
            });
        });
    });
}

void Async::break_loop() { reactor->break_loop(); }

bool Async::empty() { return active == 0; }

void Async::join() {
    if (running) {
        thread.join();
        running = false;
    }
}

/*static*/ Var<Async> Async::global() {
    static Var<Async> singleton(new Async);
    return singleton;
}

} // namespace mk
