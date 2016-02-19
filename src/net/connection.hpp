// Part of measurement-kit <https://measurement-kit.github.io/>.
// Measurement-kit is free software. See AUTHORS and LICENSE for more
// information on the copying conditions.

#ifndef MEASUREMENT_KIT_NET_CONNECTION_HPP
#define MEASUREMENT_KIT_NET_CONNECTION_HPP

#include "src/common/bufferevent.hpp"
#include <measurement_kit/common/constraints.hpp>
#include "src/common/delayed_call.hpp"
#include <measurement_kit/common/error.hpp>
#include <measurement_kit/common/logger.hpp>
#include <measurement_kit/common/poller.hpp>
#include "src/common/utils.hpp"

#include <measurement_kit/net/buffer.hpp>
#include "src/net/emitter.hpp"
#include <measurement_kit/net/transport.hpp>

#include "src/dns/query.hpp"
#include <measurement_kit/dns/response.hpp>

#include <event2/bufferevent.h>
#include <event2/event.h>

#include <stdexcept>
#include <list>

#include <string.h>

namespace mk {
namespace net {

class Connection : public Emitter, public NonMovable, public NonCopyable {
  private:
    Bufferevent bev;
    dns::Query dns_request;
    unsigned int connecting = 0;
    std::string address;
    std::string port;
    std::list<std::pair<std::string, std::string>> addrlist;
    std::string family;
    unsigned int must_resolve_ipv4 = 0;
    unsigned int must_resolve_ipv6 = 0;
    DelayedCall start_connect;
    Poller *poller = Poller::global();

    // Libevent callbacks
    static void handle_read(bufferevent *, void *);
    static void handle_write(bufferevent *, void *);
    static void handle_event(bufferevent *, short, void *);

    // Functions used when connecting
    void connect_next();
    void handle_resolve(Error, char, std::vector<std::string>);
    void resolve();
    bool resolve_internal(char);

  public:
    Connection(evutil_socket_t fd, Logger *lp = Logger::global(),
               Poller *poller = mk::get_global_poller())
        : Connection("PF_UNSPEC", "0.0.0.0", "0", poller, lp, fd) {}

    Connection(const char *af, const char *a, const char *p,
               Logger *lp = Logger::global(),
               Poller *poller = mk::get_global_poller())
        : Connection(af, a, p, poller, lp, -1) {}

    Connection(const char *, const char *, const char *, Poller *, Logger *,
               evutil_socket_t);

    ~Connection() override;

    void on_data(std::function<void(Buffer)> fn) override {
        Emitter::on_data(fn);
        if (fn) {
            enable_read();
        } else {
            disable_read();
        }
    };

    evutil_socket_t get_fileno() { return (bufferevent_getfd(this->bev)); }

    void set_timeout(double timeout) override {
        struct timeval tv, *tvp;
        tvp = mk::timeval_init(&tv, timeout);
        if (bufferevent_set_timeouts(this->bev, tvp, tvp) != 0) {
            throw std::runtime_error("cannot set timeout");
        }
    }

    void clear_timeout() override { this->set_timeout(-1); }

    void start_tls(unsigned int) {
        throw std::runtime_error("not implemented");
    }

    void send(const void *base, size_t count) override {
        if (base == NULL || count == 0) {
            throw std::runtime_error("invalid argument");
        }
        if (bufferevent_write(bev, base, count) != 0) {
            throw std::runtime_error("cannot write");
        }
    }

    void send(std::string data) override { send(data.c_str(), data.length()); }

    void send(Buffer data) override { data >> bufferevent_get_output(bev); }

    void enable_read() {
        if (bufferevent_enable(this->bev, EV_READ) != 0) {
            throw std::runtime_error("cannot enable read");
        }
    }

    void disable_read() {
        if (bufferevent_disable(this->bev, EV_READ) != 0) {
            throw std::runtime_error("cannot disable read");
        }
    }

    void close() override;
};

} // namespace net
} // namespace mk
#endif
