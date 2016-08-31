// Part of measurement-kit <https://measurement-kit.github.io/>.
// Measurement-kit is free software. See AUTHORS and LICENSE for more
// information on the copying conditions.

#include <measurement_kit/report.hpp>

namespace mk {
namespace report {

/* static */ Entry Entry::array() {
    Entry entry;
    // Manually construct an empty array by pushing
    // a value and then removing it
    entry.push_back(17.0);
    entry.erase(0);
    return entry;
}

/* static */ Entry Entry::object() {
    Entry entry;
    // Manually construct an empty object by pushing
    // a value and then removing it
    entry["foo"] = "bar";
    entry.erase("foo");
    return entry;
}

void Entry::push_back(Entry value) {
    try {
        nlohmann::json::push_back(value);
    } catch (std::domain_error &) {
        throw JsonDomainError();
    }
}

std::string Entry::dump() const {
    return nlohmann::json::dump();
}

Entry Entry::parse(const std::string &s) {
  return static_cast<Entry>(nlohmann::json::parse(s));
}

bool Entry::operator==(std::nullptr_t right) {
    return static_cast<nlohmann::json &>(*this) == right;
}

bool Entry::operator!=(std::nullptr_t right) {
    return static_cast<nlohmann::json &>(*this) != right;
}

} // namespace report
} // namespace mk
