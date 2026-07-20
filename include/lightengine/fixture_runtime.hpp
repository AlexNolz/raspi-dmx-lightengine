#pragma once

#include "lightengine/color.hpp"
#include "lightengine/dmx.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace lightengine {

class RgbWash final {
public:
    RgbWash(DmxAddress red, DmxAddress green, DmxAddress blue);

    void set_color(Rgb color) noexcept {
        color_ = color;
    }

    [[nodiscard]] Rgb color() const noexcept {
        return color_;
    }

    void render_to(DmxFrame& frame) const;

private:
    DmxAddress red_;
    DmxAddress green_;
    DmxAddress blue_;
    Rgb color_{};
};

class RgbWashBar final {
public:
    RgbWashBar(DmxAddress start_address, std::uint8_t wash_count);

    void clear() noexcept;
    void set_all(Rgb color) noexcept;
    void set_wash(std::size_t index, Rgb color);
    void render_to(DmxFrame& frame) const;

    [[nodiscard]] std::size_t size() const noexcept {
        return washes_.size();
    }

    [[nodiscard]] Rgb wash_color(std::size_t index) const;

private:
    std::vector<RgbWash> washes_;
};

}  // namespace lightengine
