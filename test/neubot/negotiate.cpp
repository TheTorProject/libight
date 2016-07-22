#define CATCH_CONFIG_MAIN

#include "src/ext/Catch/single_include/catch.hpp"
#include <measurement_kit/neubot.hpp>
#include "src/neubot/negotiate_impl.hpp"
#include "src/net/emitter.hpp"

using namespace mk;
using namespace mk::neubot::negotiate;

#ifdef ENABLE_INTEGRATION_TESTS
TEST_CASE("Test works as expected") {
    loop_with_initial_event([=]() {
        run([=](Error error) {
            REQUIRE(!error);
            break_loop();
        });

    });
}
#endif


static void fail(std::string, Callback<Error, mlabns::Reply> cb, Settings,
                 Var<Reactor>, Var<Logger>) {
    cb(MockedError(), {});
}

TEST_CASE("run() deals with mlab-ns query error") {
    run_impl<fail>(
        [](Error error) { REQUIRE(error == MockedError()); }, {},
        Reactor::global(), Logger::global()
    );
}

static void receive_no_authentication_key(Var<net::Transport>, Settings, Headers,
                                            std::string,
                                            Callback<Error, Var<http::Response>> cb,
                                            Var<Reactor> = Reactor::global(),
                                            Var<Logger> = Logger::global()) {
    Var<http::Response> response(new http::Response);
    response->status_code = 200;
    response->body = "{\"unchoked\": 0, "
                    "\"authorization\": \"antani\", "
                    "\"queue_pos\": 1, "
                    "\"real_address\": \"0.0.0.0\"}";
    cb(NoError(), response);
}

TEST_CASE("Too many negotiations", "[xx]") {
    Var<net::Transport> emitter(new net::Emitter);
    loop_negotiate<receive_no_authentication_key>( emitter,
        [](Error error) { REQUIRE(error); }, {},
        Reactor::global(), Logger::global()
    );
}

static void receive_invalid_status_code(Var<net::Transport>, Settings, Headers,
                                            std::string,
                                            Callback<Error, Var<http::Response>> cb,
                                            Var<Reactor> = Reactor::global(),
                                            Var<Logger> = Logger::global()) {
    Var<http::Response> response(new http::Response);
    response->status_code = 500;
    cb(NoError(), response);
}

TEST_CASE("Make sure that an error is passed to callback if the response "
          "status is not 200") {
    Var<net::Transport> emitter(new net::Emitter);
    loop_negotiate<receive_invalid_status_code>( emitter,
        [](Error error) { REQUIRE(error == HttpRequestFailedError()); }, {},
        Reactor::global(), Logger::global()
    );
}
