// Part of measurement-kit <https://measurement-kit.github.io/>.
// Measurement-kit is free software. See AUTHORS and LICENSE for more
// information on the copying conditions.
// =============================================================
// Based on <http://stackoverflow.com/questions/10626856>
#ifndef MEASUREMENT_KIT_COMMON_FREVERSE_HPP
#define MEASUREMENT_KIT_COMMON_FREVERSE_HPP

#include <tuple>
#include <utility>

namespace mk {

template <typename... T, std::size_t... I>
constexpr auto freverse_(std::tuple<T...> &&t, std::index_sequence<I...>) {
    return std::make_tuple(std::move(std::get<sizeof...(T) - I - 1>(t))...);
}

template <typename... T> constexpr auto freverse(std::tuple<T...> &&t) {
    //static_assert(sizeof...(T) >= 1, "tuple size is zero");
    return freverse_(std::move(t), std::make_index_sequence<sizeof...(T)>{});
}

} // namespace mk
#endif
