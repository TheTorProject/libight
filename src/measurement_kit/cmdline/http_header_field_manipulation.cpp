// Part of measurement-kit <https://measurement-kit.github.io/>.
// Measurement-kit is free software. See AUTHORS and LICENSE for more
// information on the copying conditions.

#include "../cmdline/cmdline.hpp"
#include <measurement_kit/nettests.hpp>
#include <measurement_kit/http.hpp>

#include <iostream>

namespace mk {
namespace cmdline {
namespace http_header_field_manipulation {

static const char *kv_usage =
    "usage: measurement_kit http_header_field_manipulation [-v]\n";

int main(const char *, int argc, char **argv) {
    std::string backend = "http://38.107.216.10:80";
    Settings settings;
    std::string name = argv[0];
    uint32_t verbosity = 0;
    mk::nettests::HttpHeaderFieldManipulationTest test;
    int ch;
    while ((ch = mkp_getopt(argc, argv, "b:nv")) != -1) {
        switch (ch) {
        case 'b':
            backend = mkp_optarg;
            break;
        case 'v':
            ++verbosity;
            break;
        case 'n':
            test.set_options("no_collector", true);
            break;
        default:
            std::cout << kv_usage;
            exit(1);
        }
    }
    argc -= mkp_optind;
    argv += mkp_optind;

    test
        .set_options("backend", backend)
        .set_verbosity(verbosity)
        .on_log([](uint32_t, const char *s) { std::cout << s << "\n"; })
        .run();

    return 0;
}

} // namespace http_header_field_manipulation
} // namespace cmdline
} // namespace mk
