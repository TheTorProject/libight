// Part of measurement-kit <https://measurement-kit.github.io/>.
// Measurement-kit is free software. See AUTHORS and LICENSE for more
// information on the copying conditions.

#include "src/common/utils.hpp"
#include "src/ext/strtonum.h"
#include <algorithm>
#include <arpa/inet.h>
#include <ctype.h>
#include <deque>
#include <event2/util.h>
#include <math.h>
#include <netinet/in.h>
#include <cstddef>
#include <cstring>
#include <measurement_kit/common/error.hpp>
#include <measurement_kit/common/logger.hpp>
#include <stdint.h>
#include <stdlib.h>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#include <time.h>

#define MEASUREMENT_KIT_SOCKET_INVALID -1

namespace mk {

void timeval_now(timeval *tv) {
    if (gettimeofday(tv, nullptr) != 0) {
        throw std::runtime_error("gettimeofday()");
    }
}

double time_now() {
    timeval tv;
    timeval_now(&tv);
    double result = tv.tv_sec + tv.tv_usec / (double)1000000.0;
    return result;
}

void utc_time_now(struct tm *utc) {
    time_t tv;
    tv = time(nullptr);
    gmtime_r(&tv, utc);
}

std::string timestamp(const struct tm *t) {
    char result[30];
    if (strftime(result, sizeof(result), "%Y-%m-%d %H:%M:%S", t) == 0) {
        throw std::runtime_error("strftime()");
    };
    return std::string(result);
}

timeval *timeval_init(timeval *tv, double delta) {
    if (delta < 0) {
        return nullptr;
    }
    tv->tv_sec = (time_t)floor(delta);
    tv->tv_usec = (suseconds_t)((delta - floor(delta)) * 1000000);
    return tv;
}

int storage_init(sockaddr_storage *storage, socklen_t *salen,
                 const char *family, const char *address, const char *port) {
    int _family;
    /* TODO: support also AF_INET, AF_INET6, ... */
    if (strcmp(family, "PF_INET") == 0) {
        _family = PF_INET;
    } else if (strcmp(family, "PF_INET6") == 0) {
        _family = PF_INET6;
    } else {
        warn("utils:storage_init: invalid family");
        return -1;
    }
    return storage_init(storage, salen, _family, address, port);
}

int storage_init(sockaddr_storage *storage, socklen_t *salen, int _family,
                 const char *address, const char *port) {
    const char *errstr;
    int _port = (int)measurement_kit_strtonum(port, 0, 65535, &errstr);
    if (errstr != nullptr) {
        warn("utils:storage_init: invalid port");
        return -1;
    }
    return storage_init(storage, salen, _family, address, _port);
}

int storage_init(sockaddr_storage *storage, socklen_t *salen, int _family,
                 const char *address, int _port) {
    int result;

    debug("utils:storage_init - enter");

    if (_port < 0 || _port > 65535) {
        warn("utils:storage_init: invalid port");
        return -1;
    }

    memset(storage, 0, sizeof(*storage));
    switch (_family) {

    case PF_INET6: {
        sockaddr_in6 *sin6 = (sockaddr_in6 *)storage;
        sin6->sin6_family = AF_INET6;
        sin6->sin6_port = htons(_port);
        if (address != nullptr) {
            result = inet_pton(AF_INET6, address, &sin6->sin6_addr);
            if (result != 1) {
                warn("utils:storage_init: invalid addr");
                return -1;
            }
        } else {
            sin6->sin6_addr = in6addr_any;
        }
        *salen = sizeof(sockaddr_in6);
        break;
    }

    case PF_INET: {
        sockaddr_in *sin = (sockaddr_in *)storage;
        sin->sin_family = AF_INET;
        sin->sin_port = htons(_port);
        if (address != nullptr) {
            result = inet_pton(AF_INET, address, &sin->sin_addr);
            if (result != 1) {
                warn("utils:storage_init: invalid addr");
                return -1;
            }
        } else {
            sin->sin_addr.s_addr = INADDR_ANY;
        }
        *salen = sizeof(sockaddr_in);
        break;
    }

    default:
        throw std::runtime_error("invalid case");
    }

    debug("utils:storage_init - ok");
    return 0;
}

evutil_socket_t socket_create(int domain, int type, int protocol) {
    evutil_socket_t filedesc;
    int result;

    debug("utils:socket - enter");

    filedesc = socket(domain, type, protocol);
    if (filedesc == MEASUREMENT_KIT_SOCKET_INVALID) {
        warn("utils:socket: cannot create socket");
        return MEASUREMENT_KIT_SOCKET_INVALID;
    }

    result = evutil_make_socket_nonblocking(filedesc);
    if (result != 0) {
        warn("utils:socket: cannot make nonblocking");
        (void)evutil_closesocket(filedesc);
        return MEASUREMENT_KIT_SOCKET_INVALID;
    }

    debug("utils:socket - ok");
    return filedesc;
}

// See <http://stackoverflow.com/questions/440133/>
std::string random_within_charset(std::string charset, size_t length) {
    if (charset.size() < 1) {
        throw ValueError();
    }
    auto randchar = [&charset]() {
        int rand = 0;
        evutil_secure_rng_get_bytes(&rand, sizeof (rand));
        return charset[rand % charset.size()];
    };
    std::string str(length, 0);
    std::generate_n(str.begin(), length, randchar);
    return str;
}

std::string random_printable(size_t length) {
    return random_within_charset(
            " !\"#$%&\'()*+,-./"         // before numbers
            "0123456789"                 // numbers
            ":;<=>?@"                    // after numbers
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ" // uppercase
            "[\\]^_`"                    // between upper and lower
            "abcdefghijklmnopqrstuvwxyz" // lowercase
            "{|}~"                       // final
        , length);
}

std::string random_str(size_t length) {
    return random_within_charset(
            "0123456789"                  // numbers
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"  // uppercase
            "abcdefghijklmnopqrstuvwxyz"  // lowercase
        , length);
}

std::string random_str_uppercase(size_t length) {
    return random_within_charset(
            "0123456789"                  // numbers
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"  // uppercase
        , length);
}

std::string unreverse_ipv6(std::string s) {
    size_t i = 0, added = 0;
    std::deque<char> r;
    for (; i < s.size(); ++i) {
        if ((i % 2) == 0) {
            if (!isxdigit(s[i])) {
                break;
            }
            r.push_front(s[i]);
            if ((++added % 4) == 0 && added <= 28) {
                r.push_front(':');
            }
        } else {
            if (s[i] != '.') {
                return "";
            }
        }
    }
    if (s.substr(i) != "ip6.arpa" && s.substr(i) != "ip6.arpa.") {
        return "";
    }
    return std::string(r.begin(), r.end());
}

std::string unreverse_ipv4(std::string s) {
    std::deque<char> r, t;
    size_t i = 0, seen = 0;
    unsigned cur = 0;
    for (; i < s.size(); ++i) {
        if (s[i] == '.') {
            if (cur > 255) {
                return "";
            }
            if (seen++ > 0) {
                r.push_front('.');
            }
            r.insert(r.begin(), t.begin(), t.end());
            t.clear();
            cur = 0;
        } else if (isdigit(s[i])) {
            t.push_back(s[i]);
            char tmpstr[] = {s[i], '\0'};
            cur = cur * 10 + atoi(tmpstr);
        } else {
            break;
        }
    }
    if (s.substr(i) != "in-addr.arpa" && s.substr(i) != "in-addr.arpa.") {
        return "";
    }
    return std::string(r.begin(), r.end());
}

} // namespace mk
