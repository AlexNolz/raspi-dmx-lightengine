#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>

namespace lightengine {

constexpr std::size_t dmx_channel_count = 512;
using DmxFrame = std::array<std::uint8_t, dmx_channel_count>;

class DmxAddress final {
public:
    explicit constexpr DmxAddress(std::uint16_t value) : value_{value} {
        if (value < 1 || value > dmx_channel_count) {
            throw std::out_of_range{"DMX address must be in range 1..512"};
        }
    }

    [[nodiscard]] constexpr std::uint16_t value() const noexcept {
        return value_;
    }

    [[nodiscard]] constexpr std::size_t zero_based() const noexcept {
        return static_cast<std::size_t>(value_ - 1U);
    }

private:
    std::uint16_t value_;
};

class ArtNetUniverse final {
public:
    explicit constexpr ArtNetUniverse(std::uint16_t value) : value_{value} {
        if (value > 32767U) {
            throw std::out_of_range{"Art-Net universe must be in range 0..32767"};
        }
    }

    [[nodiscard]] constexpr std::uint16_t value() const noexcept {
        return value_;
    }

private:
    std::uint16_t value_;
};

}  // namespace lightengine
