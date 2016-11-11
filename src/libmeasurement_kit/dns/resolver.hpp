// Measurement-kit is free software. See AUTHORS and LICENSE for more
// information on the copying conditions.

#include <measurement_kit/common.hpp>
#include <measurement_kit/dns.hpp>

#include <thread>
/// XXX use versions compatible with windows
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>

namespace mk {
namespace dns {

class ResolverContext {
  public:
    QueryClass dns_class;
    QueryType dns_type;
    std::string name;
    Callback<Error, Var<Message>> cb;
    Settings settings;
    Var<Reactor> reactor;
    Var<Logger> logger;

    struct addrinfo hints;
    struct addrinfo *servinfo = nullptr;

    Var<Message> message{new Message};

    ResolverContext(QueryClass dns_c, QueryType dns_t, std::string n,
                    Callback<Error, Var<Message>> c, Settings s, Var<Reactor> r,
                    Var<Logger> l) {
        dns_class = dns_c;
        dns_type = dns_t;
        name = n;
        cb = c;
        settings = s;
        reactor = r;
        logger = l;
        memset(&hints, 0, sizeof(hints));
    }

    ~ResolverContext() {
        if (servinfo != nullptr) {
            freeaddrinfo(servinfo);
        }
    }
};

static inline void resolve_async(ResolverContext *ctx) {
    Callback<Error, Var<Message>> callback = ctx->cb;
    Var<Message> message_copy;
    struct addrinfo *p;

    int error = getaddrinfo(ctx->name.c_str(), nullptr, &(ctx->hints),
                            &(ctx->servinfo));
    if (error) {
        // check the error variable and return the correct error
        ctx->logger->warn(gai_strerror(error));
        ctx->reactor->call_soon(
            [callback]() { callback(ResolverError(), nullptr); });
        delete ctx;
        return;
    }

    if (ctx->servinfo == nullptr) {
        ctx->reactor->call_soon(
            [callback]() { callback(ResolverError(), nullptr); });
        delete ctx;
        return;
    }

    void *addr_ptr;
    char address[128];
    std::vector<Answer> answers;
    for (p = ctx->servinfo; p != nullptr; p = p->ai_next) {
        Answer answer;
        answer.name = ctx->name;
        answer.qclass = ctx->dns_class;
        if (p->ai_family == AF_INET) {
            answer.type = QueryTypeId::A;
            addr_ptr = &((struct sockaddr_in *)p->ai_addr)->sin_addr;
        } else if (p->ai_family == AF_INET6) {
            answer.type = QueryTypeId::AAAA;
            addr_ptr = &((struct sockaddr_in6 *)p->ai_addr)->sin6_addr;
        } else {
            ctx->reactor->call_soon(
                [callback]() { callback(ResolverError(), nullptr); });
            delete ctx;
            return;
        }
        if (inet_ntop(p->ai_family, addr_ptr, address, sizeof(address)) ==
            NULL) {
            ctx->logger->warn("dns: unexpected inet_ntop failure");
            throw std::runtime_error("Unexpected inet_ntop failure");
        }
        if (p->ai_family == AF_INET) {
            answer.ipv4 = std::string(address);
        } else if (p->ai_family == AF_INET6) {
            answer.ipv6 = std::string(address);
        }
        answers.push_back(answer);
    }
    ctx->message->answers = answers;
    message_copy = ctx->message;
    ctx->reactor->call_soon(
        [callback, message_copy]() { callback(NoError(), message_copy); });
    delete ctx;
}

inline void system_resolver(QueryClass dns_class, QueryType dns_type,
                            std::string name, Callback<Error, Var<Message>> cb,
                            Settings settings, Var<Reactor> reactor,
                            Var<Logger> logger) {
    ResolverContext *ctx = new ResolverContext(dns_class, dns_type, name, cb,
                                               settings, reactor, logger);
    Query query;
    ctx->hints.ai_flags = AI_ADDRCONFIG;
    ctx->hints.ai_socktype = SOCK_STREAM;
    ctx->hints.ai_protocol = 0;
    ctx->hints.ai_canonname = NULL;
    ctx->hints.ai_addr = NULL;
    ctx->hints.ai_next = NULL;

    if (dns_class != QueryClassId::IN) {
        cb(UnsupportedClassError(), nullptr);
        return;
    }

    if (dns_type == QueryTypeId::A) {
        ctx->hints.ai_family = AF_UNSPEC;
    } else if (dns_type == QueryTypeId::AAAA) {
        ctx->hints.ai_family = AF_INET6;
    } else {
        cb(UnsupportedTypeError(), nullptr);
        return;
    }

    query.type = dns_type;
    query.qclass = dns_class;
    query.name = name;

    ctx->message->queries.push_back(query);

    std::thread res_thread(resolve_async, ctx);
    /// XXX study a way to mantain a reference to the thread
    res_thread.detach();
}

} // namespace dns
} // namespace mk
