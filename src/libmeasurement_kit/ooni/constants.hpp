// Part of measurement-kit <https://measurement-kit.github.io/>.
// Measurement-kit is free software. See AUTHORS and LICENSE for more
// information on the copying conditions.
#ifndef SRC_LIBMEASUREMENT_KIT_OONI_CONSTANTS_HPP
#define SRC_LIBMEASUREMENT_KIT_OONI_CONSTANTS_HPP

#include <measurement_kit/http.hpp>

#include <set>

namespace mk {
namespace ooni {
namespace constants {

// These are very common server headers that we don't consider when checking
// between control and experiment.
const std::set<std::string> COMMON_SERVER_HEADERS = {
  "date",
  "content-type",
  "server",
  "cache-control",
  "vary",
  "set-cookie",
  "location",
  "expires",
  "x-powered-by",
  "content-encoding",
  "last-modified",
  "accept-ranges",
  "pragma",
  "x-frame-options",
  "etag",
  "x-content-type-options",
  "age",
  "via",
  "p3p",
  "x-xss-protection",
  "content-language",
  "cf-ray",
  "strict-transport-security",
  "link",
  "x-varnish"
};


const http::Headers COMMON_CLIENT_HEADERS = {
  {
    "User-Agent",
    "Mozilla/5.0 (Windows NT 6.1) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/47.0.2526.106 Safari/537.36"
  },
  {
    "Accept-Language", "en-US;q=0.8,en;q=0.5"
  },
  {
    "Accept", "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8"
  }
};


} // namespace constants
} // namespace ooni
} // namespace mk

#endif
