// Part of measurement-kit <https://measurement-kit.github.io/>.
// Measurement-kit is free software. See AUTHORS and LICENSE for more
// information on the copying conditions.
#ifndef MEASUREMENT_KIT_COMMON_VERSION_HPP
#define MEASUREMENT_KIT_COMMON_VERSION_HPP

// File generated by `./autogen.sh` containing the exact version
#include <measurement_kit/common/git_version.hpp>

// Note: we use semantic versioning (see: http://semver.org/)
#define MK_VERSION "0.7.0-alpha.2"
#define MEASUREMENT_KIT_VERSION MK_VERSION /* Backward compatibility */

#ifdef __cplusplus
extern "C" {
#endif

const char *mk_version(void);
const char *mk_version_full(void);
const char *mk_openssl_version(void);
const char *mk_libevent_version(void);

#ifdef __cplusplus
} // namespace mk
#endif
#endif
