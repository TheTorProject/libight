// Part of measurement-kit <https://measurement-kit.github.io/>.
// Measurement-kit is free software. See AUTHORS and LICENSE for more
// information on the copying conditions.

#include <measurement_kit/nettests.hpp>
#include <measurement_kit/ndt.hpp>

namespace mk {
namespace nettests {

MultiNdtTest::MultiNdtTest() : BaseTest() {
    runnable.reset(new MultiNdtRunnable);
    runnable->options["save_real_probe_ip"] = true;
    runnable->options["dns/engine"] = "system";
    runnable->test_name = "multi_ndt";
    runnable->test_version = "0.0.6";  /* Forked from `ndt` v0.0.4 */
}

/*
 * Note: in the following function and other functions below we're going to
 * take advantage of the fact that `Entry` is a JSON type, thus we are going
 * to return, e.g., `nullptr` to indicate error and otherwise a double.
 *
 * This behavior is correct C++ because the constructor of `Entry` can take
 * in input any valid JSON (`null`, numbers, strings, lists, objects).
 */

static report::Entry compute_ping(report::Entry &test_s2c, Var<Logger> logger) {

    try {
        // Note: do static cast to make sure it's convertible to a double
        return static_cast<double>(test_s2c["web100_data"]["MinRTT"]);
    } catch (const std::exception &) {
        logger->warn("Cannot access Web100 data");
        /* Fallthrough to next method of computing RTT */
    }

    std::vector<double> rtts;
    try {
        // XXX Needed to add temp variable because otherwise it did not compile
        std::vector<double> temp = test_s2c["connect_times"];
        rtts = temp;
    } catch (const std::exception &) {
        logger->warn("Cannot access connect times");
        /* Fallthrough to the following check that will fail */ ;
    }
    if (rtts.size() <= 0) {
        logger->warn("Did not find any reliable way to compute RTT");
        return nullptr; /* We cannot compute the RTT */
    }

    double sum = 0.0;
    for (auto &rtt: rtts) {
        sum += rtt * 1000.0 /* To milliseconds! */;
    }
    return sum / rtts.size();  /* Division by zero excluded above */
}

static report::Entry compute_download_speed(report::Entry &test_s2c,
                                            Var<Logger> logger) {
    /*
     * This algorithm computes the speed in a way that is similar to the one
     * implemented by OOKLA, as documented here:
     *
     *    http://www.ookla.com/support/a21110547/what-is-the-test-flow-and-methodology-for-the-speedtest
     */
    try {
        std::vector<double> speeds;
        for (auto &x: test_s2c["receiver_data"]) {
            speeds.push_back(x[1]);
        }
        std::sort(speeds.begin(), speeds.end());
        std::vector<double> good_speeds(
            // Note: going beyond vector limits would raise
            // a std::length_error exception
            speeds.begin() + 6, speeds.end() - 2
        );
        double sum = 0.0;
        for (auto &x : good_speeds) {
            sum += x;
        };
        if (good_speeds.size() <= 0) {
            logger->warn("The vector of good speeds is empty");
            return nullptr;
        }
        return sum / good_speeds.size();
    } catch (const std::exception &) {
        logger->warn("Cannot compute download speed");
        // FALLTHROUGH
    }
    return nullptr;
}

static report::Entry compute_stats(report::Entry &root, std::string key,
                                   Var<Logger> logger) {
    report::Entry stats;
    report::Entry test_s2c;
    try {
        test_s2c = root[key]["test_s2c"][0] /* We know it's just one entry */;
    } catch (const std::exception &) {
        logger->warn("cannot access root[\"%s\"][\"test_s2c\"][0]",
                     key.c_str());
        return nullptr;  /* Cannot compute this stat */
    }
    stats["ping"] = compute_ping(test_s2c, logger);
    stats["download"] = compute_download_speed(test_s2c, logger);
    stats["fastest_test"] = key;
    return stats;
}

static void compute_simple_stats(report::Entry &entry, Var<Logger> logger) {
    report::Entry single = compute_stats(entry, "single_stream", logger);
    report::Entry multi = compute_stats(entry, "multi_stream", logger);
    report::Entry selected;

    /*
     * Here we basically pick up the fastest of the two tests.
     */
    if (single["ping"] != nullptr and multi["ping"] != nullptr) {
        if (single["download"] != nullptr and multi["download"] != nullptr) {
            double singled = single["download"];
            double multid = multi["download"];
            if (singled > multid) {
                selected = single;
            } else {
                selected = multi;
            }
        } else if (single["download"] != nullptr) {
            logger->warn("Multi-stream download is null");
            selected = single;
        } else if (multi["download"] != nullptr) {
            logger->warn("Single-stream download is null");
            selected = multi;
        } else {
            logger->warn("Single- and multi-stream download are null");
        }
    } else if (single["ping"] != nullptr) {
        logger->warn("Multi-stream ping is null");
        selected = single;
    } else if (multi["ping"] != nullptr) {
        logger->warn("Single-stream ping is null");
        selected = multi;
    } else {
        logger->warn("Single- and multi-stream ping are null");
    }

    entry["simple"] = selected;
}

static void compute_advanced_stats(report::Entry &entry) {
    report::Entry test_s2c =
        entry["single_stream"]["test_s2c"][0] /* We know it's just one entry */;

    // See: https://github.com/ndt-project/ndt/wiki/NDTTestMethodology#computed-variables

    double SndLimTimeRwin = test_s2c["web100_data"]["SndLimTimeRwin"];
    double SndLimTimeCwnd = test_s2c["web100_data"]["SndLimTimeCwnd"];
    double SndLimTimeSender = test_s2c["web100_data"]["SndLimTimeSender"];
    double TotalTestTime = SndLimTimeRwin + SndLimTimeCwnd + SndLimTimeSender;

    double CongestionSignals = test_s2c["web100_data"]["CongestionSignals"];
    double PktsOut = test_s2c["web100_data"]["PktsOut"];
    double PacketLoss = 0.0;
    if (PktsOut > 0.0) {
        PacketLoss = CongestionSignals / PktsOut;
    }
    entry["advanced"]["PacketLoss"] = PacketLoss;

    double DupAcksIn = test_s2c["web100_data"]["DupAcksIn"];
    double AckPktsIn = test_s2c["web100_data"]["AckPktsIn"];
    double OutOfOrder = 0.0;
    if (AckPktsIn > 0.0) {
        OutOfOrder = DupAcksIn / AckPktsIn;
    }
    entry["advanced"]["OutOfOrder"] = OutOfOrder;

    double SumRTT = test_s2c["web100_data"]["SumRTT"];
    double CountRTT = test_s2c["web100_data"]["CountRTT"];
    double AvgRTT = 0.0;
    if (CountRTT > 0.0) {
        AvgRTT = SumRTT / CountRTT;
    }
    entry["advanced"]["AvgRTT"] = AvgRTT;

    double CongestionLimited = 0.0;
    if (TotalTestTime > 0.0) {
        CongestionLimited = SndLimTimeCwnd / TotalTestTime;
    }
    entry["advanced"]["CongestionLimited"] = CongestionLimited;

    double ReceiverLimited = 0.0;
    if (TotalTestTime > 0.0) {
        ReceiverLimited = SndLimTimeRwin / TotalTestTime;
    }
    entry["advanced"]["ReceiverLimited"] = ReceiverLimited;

    double SenderLimited = 0.0;
    if (TotalTestTime > 0.0) {
        SenderLimited = SndLimTimeSender / TotalTestTime;
    }
    entry["advanced"]["SenderLimited"] = SenderLimited;

    entry["advanced"]["MinRTT"] = test_s2c["web100_data"]["MinRTT"];
    entry["advanced"]["MaxRTT"] = test_s2c["web100_data"]["MaxRTT"];
    entry["advanced"]["MSS"] = test_s2c["web100_data"]["CurMSS"];
    entry["advanced"]["FastRetran"] = test_s2c["web100_data"]["FastRetran"];
    entry["advanced"]["Timeouts"] = test_s2c["web100_data"]["Timeouts"];
}

void MultiNdtRunnable::main(std::string, Settings ndt_settings,
                            Callback<Var<report::Entry>> cb) {
    // Note: `options` is the class attribute and `settings` is instead a
    // possibly modified copy of the `options` object

    Var<report::Entry> ndt_entry(new report::Entry);
    (*ndt_entry)["failure"] = nullptr;
    ndt_settings["test_suite"] = MK_NDT_DOWNLOAD;
    logger->progress(0.0, "Starting single-stream test");
    logger->set_progress_scale(0.5);
    ndt::run(ndt_entry, [=](Error ndt_error) {
        if (ndt_error) {
            (*ndt_entry)["failure"] = ndt_error.as_ooni_error();
            logger->warn("Test failed: %s", ndt_error.explain().c_str());
            // FALLTHROUGH
        }

        Var<report::Entry> neubot_entry(new report::Entry);
        (*neubot_entry)["failure"] = nullptr;
        Settings neubot_settings{ndt_settings.begin(), ndt_settings.end()};
        neubot_settings["test_suite"] = MK_NDT_DOWNLOAD_EXT;
        neubot_settings["mlabns_tool_name"] = "neubot";
        logger->set_progress_offset(0.5);
        logger->progress(0.0, "Starting multi-stream test");
        ndt::run(neubot_entry, [=](Error neubot_error) {
            logger->progress(1.0, "Test completed");
            if (neubot_error) {
                (*neubot_entry)["failure"] = neubot_error.as_ooni_error();
                logger->warn("Test failed: %s", neubot_error.explain().c_str());
                // FALLTHROUGH
            }
            Var<report::Entry> overall_entry(new report::Entry);
            (*overall_entry)["failure"] = nullptr;
            (*overall_entry)["multi_stream"] = *neubot_entry;
            (*overall_entry)["single_stream"] = *ndt_entry;
            if (ndt_error or neubot_error) {
                Error overall_error = SequentialOperationError();
                overall_error.child_errors.push_back(
                    Var<Error>{new Error{ndt_error}}
                );
                overall_error.child_errors.push_back(
                    Var<Error>{new Error{neubot_error}}
                );
                (*overall_entry)["failure"] = overall_error.as_ooni_error();
                // FALLTHROUGH
            }
            try {
                compute_simple_stats(*overall_entry, logger);
            } catch (const std::exception &) {
                /* Just in case */ ;
            }
            try {
                compute_advanced_stats(*overall_entry);
            } catch (const std::exception &) {
                /* Just in case */ ;
            }
            cb(overall_entry);
        }, neubot_settings, reactor, logger);
    }, ndt_settings, reactor, logger);
}

} // namespace nettests
} // namespace mk
