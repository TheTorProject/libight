// Part of measurement-kit <https://measurement-kit.github.io/>.
// Measurement-kit is free software. See AUTHORS and LICENSE for more
// information on the copying conditions.
#ifndef SRC_REPORT_FILE_REPORTER_HPP
#define SRC_REPORT_FILE_REPORTER_HPP

#include <fstream>
#include "src/report/base_reporter.hpp"

namespace mk {
namespace report {

class FileReporter : public BaseReporter {
  public:
    std::string filename;

    void open() override;
    void write_entry(json &entry) override;
    void close() override;

  private:
    std::ofstream file;
};

} // namespace report
} // namespace mk
#endif
