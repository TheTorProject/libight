// Part of measurement-kit <https://measurement-kit.github.io/>.
// Measurement-kit is free software. See AUTHORS and LICENSE for more
// information on the copying conditions.

#include "src/common/utils.hpp"
#include "src/ooni/utils.hpp"
#include <measurement_kit/ooni.hpp>

namespace mk {
namespace ooni {

void OoniTest::run_next_measurement(Callback<Error> cb) {
    logger->debug("net_test: running next measurement");
    std::string next_input;
    std::getline(*input_generator, next_input);
    if (input_generator->eof()) {
        logger->debug("net_test: reached end of input");
        cb(NoError());
        return;
    }
    if (!input_generator->good()) {
        logger->warn("net_test: I/O error reading input");
        cb(FileIoError());
        return;
    }

    logger->debug("net_test: creating entry");
    struct tm measurement_start_time;
    double start_time;
    mk::utc_time_now(&measurement_start_time);
    start_time = mk::time_now();

    logger->debug("net_test: calling setup");
    setup(next_input);

    logger->debug("net_test: running with input %s", next_input.c_str());
    main(next_input, options, [=](report::Entry test_keys) {
        report::Entry entry;
        entry["test_keys"] = test_keys;
        entry["test_keys"]["client_resolver"] = resolver_ip;
        entry["input"] = next_input;
        entry["measurement_start_time"] =
            *mk::timestamp(&measurement_start_time);
        entry["test_runtime"] = mk::time_now() - start_time;

        logger->debug("net_test: tearing down");
        teardown(next_input);

        Error error = file_report.write_entry(entry);
        if (error) {
            cb(error);
            return;
        }
        if (entry_cb) {
            entry_cb(entry.dump());
        }
        logger->debug("net_test: written entry");

        reactor->call_soon([=]() { run_next_measurement(cb); });
    });
}

void OoniTest::geoip_lookup(Callback<> cb) {
    // This is to ensure that when calling multiple times geoip_lookup we
    // always reset the probe_ip, probe_asn and probe_cc values.
    probe_ip = "127.0.0.1";
    probe_asn = "AS0";
    probe_cc = "ZZ";
    ip_lookup(
        [=](Error err, std::string ip) {
            if (err) {
                logger->warn("ip_lookup() failed: error code: %d", err.code);
            } else {
                logger->info("probe ip: %s", ip.c_str());
                if (options.get("save_real_probe_ip", false)) {
                    logger->debug("saving user's real ip on user's request");
                    probe_ip = ip;
                }
                std::string country_p =
                    options.get("geoip_country_path", std::string{});
                std::string asn_p =
                    options.get("geoip_asn_path", std::string{});
                if (country_p == "" or asn_p == "") {
                    logger->warn("geoip files not configured; skipping");
                } else {
                    ErrorOr<nlohmann::json> res = geoip(ip, country_p, asn_p);
                    if (!!res) {
                        logger->debug("GeoIP result: %s", res->dump().c_str());
                        // Since `geoip()` sets defaults before querying, the
                        // following accesses of json should not fail unless for
                        // programmer error after refactoring. In that case,
                        // better to let the exception unwind than just print
                        // a warning, because the former is easier to notice
                        // and therefore fix during development
                        probe_asn = (*res)["asn"];
                        logger->info("probe_asn: %s", probe_asn.c_str());
                        probe_cc = (*res)["country_code"];
                        logger->info("probe_cc: %s", probe_cc.c_str());
                    }
                }
            }
            cb();
        },
        options, reactor, logger);
}

Error OoniTest::open_report() {
    file_report.test_name = test_name;
    file_report.test_version = test_version;
    file_report.test_start_time = test_start_time;

    file_report.options = options;

    file_report.probe_ip = probe_ip;
    file_report.probe_cc = probe_cc;
    file_report.probe_asn = probe_asn;

    if (output_filepath == "") {
        output_filepath = generate_output_filepath();
    }
    file_report.filename = output_filepath;
    return file_report.open();
}

std::string OoniTest::generate_output_filepath() {
    int idx = 0;
    std::stringstream filename;
    while (true) {
        filename.str("");
        filename.clear();

        char timestamp[100];
        strftime(timestamp, sizeof(timestamp), "%FT%H%M%SZ", &test_start_time);
        filename << "report-" << test_name << "-";
        filename << timestamp << "-" << idx << ".json";

        std::ifstream output_file(filename.str().c_str());
        // If a file called this way already exists we increment the counter
        if (output_file.good()) {
            output_file.close();
            idx++;
            continue;
        }
        break;
    }
    return filename.str();
}

void OoniTest::begin(Callback<Error> cb) {
    if (begin_cb) {
        begin_cb();
    }
    mk::utc_time_now(&test_start_time);
    geoip_lookup([=]() {
        resolver_lookup([=](Error error, std::string resolver_ip_) {
            if (!error) {
                resolver_ip = resolver_ip_;
            } else {
                logger->debug("failed to lookup resolver ip");
            }
            error = open_report();
            if (error) {
                cb(error);
                return;
            }
            if (needs_input) {
                if (input_filepath == "") {
                    logger->warn("an input file is required");
                    cb(MissingRequiredInputFileError());
                    return;
                }
                input_generator.reset(new std::ifstream(input_filepath));
                if (!input_generator->good()) {
                    logger->warn("cannot read input file");
                    cb(CannotOpenInputFileError());
                    return;
                }
            } else {
                input_generator.reset(new std::istringstream("\n"));
            }
            run_next_measurement(cb);
        }, options, reactor, logger);
    });
}

void OoniTest::end(Callback<Error> cb) {
    if (end_cb) {
        end_cb();
    }
    Error error = file_report.close();
    if (error) {
        cb(error);
        return;
    }
    collector::submit_report(
        output_filepath,
        options.get(
            // Note: by default we use the testing collector URL because otherwise
            // testing runs would be collected creating noise and using resources
            "collector_base_url",
            collector::testing_collector_url()
        ),
        [=](Error error) { cb(error); }, options, reactor, logger);
}

} // namespace ooni
} // namespace mk
