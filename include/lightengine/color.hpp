#pragma once

#include <cstdint>

namespace lightengine {

struct Rgb final {
    std::uint8_t r{};
    std::uint8_t g{};
    std::uint8_t b{};

    bool operator==(const Rgb&) const = default;
};

}  // namespace lightengine
