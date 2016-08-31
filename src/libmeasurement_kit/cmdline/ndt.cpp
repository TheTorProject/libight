// Part of measurement-kit <https://measurement-kit.github.io/>.
// Measurement-kit is free software. See AUTHORS and LICENSE for more
// information on the copying conditions.

#include <cstdlib>
#include <cstring>
#include <functional>
#include <iostream>
#include <measurement_kit/cmdline.hpp>
#include <measurement_kit/common.hpp>
#include <measurement_kit/ndt.hpp>
#include <string>
#include <unistd.h>

namespace mk {
namespace cmdline {
namespace ndt {

using namespace mk::ndt;

static const char *kv_usage =
    "usage: measurement_kit ndt [-v] [-C /path/to/ca.bundle] [-p port]\n"
    "                           [-T download|none|upload] [host]\n";

int main(const char *, int argc, char **argv) {

    NdtTest test;
    int ch;
    while ((ch = getopt(argc, argv, "C:p:T:v")) != -1) {
        switch (ch) {
        case 'C':
            test.set_options("net/ca_bundle_path", optarg);
            break;
        case 'p':
            test.set_options("port", optarg);
            break;
        case 'T':
            if (strcmp(optarg, "download") == 0) {
                test.set_options("test_suite", MK_NDT_DOWNLOAD);
            } else if (strcmp(optarg, "none") == 0) {
                test.set_options("test_suite", 0);
            } else if (strcmp(optarg, "upload") == 0) {
                test.set_options("test_suite", MK_NDT_UPLOAD);
            } else {
                warn("invalid parameter for -T option: %s", optarg);
                exit(1);
            }
            break;
        case 'v':
            test.increase_verbosity();
            break;
        default:
            std::cout << kv_usage;
            exit(1);
        }
    }
    argc -= optind;
    argv += optind;

    if (argc > 1) {
        std::cout << kv_usage;
        exit(1);
    } else if (argc == 1) {
        test.set_options("address", argv[0]);
    }

    test
        .set_options("geoip_country_path", "test/fixtures/GeoIP.dat")
        .set_options("geoip_asn_path", "test/fixtures/GeoIPASNum.dat")
        .run();

    return 0;
}

} // namespace ndt
} // namespace cmdline
} // namespace mk
