// Part of measurement-kit <https://measurement-kit.github.io/>.
// Measurement-kit is free software. See AUTHORS and LICENSE for more
// information on the copying conditions.
#ifndef SRC_LIBMEASUREMENT_KIT_NET_SSL_CONTEXT_IMPL_HPP
#define SRC_LIBMEASUREMENT_KIT_NET_SSL_CONTEXT_IMPL_HPP

#include "../net/builtin_ca_bundle.hpp"
#include "../net/ssl_context.hpp"
#include <cassert>
#include <openssl/err.h>

namespace mk {
namespace net {

template <MK_MOCK(SSL_new)>
ErrorOr<SSL *> make_ssl(SSL_CTX *ctx, std::string hostname) {
    assert(ctx != nullptr);
    SSL *ssl = SSL_new(ctx);
    if (ssl == nullptr) {
        warn("ssl: failed to call SSL_new");
        return SslNewError();
    }
    SSL_set_tlsext_host_name(ssl, hostname.c_str());
    return ssl;
}

static void initialize_ssl() {
    static bool ssl_initialized = false;
    if (!ssl_initialized) {
        SSL_library_init();
        ERR_load_crypto_strings();
        SSL_load_error_strings();
        OpenSSL_add_all_algorithms();
        ssl_initialized = true;
    }
}

template <MK_MOCK(SSL_CTX_new), MK_MOCK(SSL_CTX_load_verify_locations)
#ifdef LIBRESSL_VERSION_NUMBER
          , MK_MOCK(SSL_CTX_load_verify_mem)
#endif
          >
ErrorOr<SSL_CTX *> make_ssl_ctx(std::string path) {

    debug("ssl: creating ssl context with bundle %s", path.c_str());
    initialize_ssl();

    SSL_CTX *ctx = SSL_CTX_new(TLSv1_client_method());
    if (ctx == nullptr) {
        debug("ssl: failed to create SSL_CTX");
        return SslCtxNewError();
    }

    if (path != "") {
        if (!SSL_CTX_load_verify_locations(ctx, path.c_str(), nullptr)) {
            debug("ssl: failed to load verify location");
            SSL_CTX_free(ctx);
            return SslCtxLoadVerifyLocationsError();
        }
    } else {
#ifdef LIBRESSL_VERSION_NUMBER
        std::vector<uint8_t> bundle = builtin_ca_bundle();
        if (!SSL_CTX_load_verify_mem(ctx, bundle.data(), bundle.size())) {
            debug("ssl: failed to load default ca bundle");
            SSL_CTX_free(ctx);
            return SslCtxLoadVerifyLocationsError();
        }
/* FALLTHROUGH */
#else
        return MissingCaBundlePathError();
#endif
    }

    SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, nullptr);
    return ctx;
}

} // namespace libevent
} // namespace mk
#endif
