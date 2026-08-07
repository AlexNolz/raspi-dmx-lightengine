#pragma once

#include <cstdint>

namespace lightengine {

struct Rgb final {
    std::uint8_t r{};
    std::uint8_t g{};
    std::uint8_t b{};

    bool operator==(const Rgb& other) const {
        return r == other.r && g == other.g && b == other.b;
    }
};

}  // namespace lightengine
