#include "lightengine/fixture_runtime.hpp"

#include <stdexcept>

namespace lightengine {

RgbWash::RgbWash(DmxAddress red, DmxAddress green, DmxAddress blue)
    : red_{red}, green_{green}, blue_{blue} {}

void RgbWash::render_to(DmxFrame& frame) const {
    frame.at(red_.zero_based()) = color_.r;
    frame.at(green_.zero_based()) = color_.g;
    frame.at(blue_.zero_based()) = color_.b;
}

RgbWashBar::RgbWashBar(const DmxAddress start_address, const std::uint8_t wash_count) {
    washes_.reserve(wash_count);
    for (std::uint8_t index = 0; index < wash_count; ++index) {
        const auto red = static_cast<std::uint16_t>(start_address.value() + static_cast<std::uint16_t>(index) * 3U);
        washes_.emplace_back(DmxAddress{red}, DmxAddress{static_cast<std::uint16_t>(red + 1U)}, DmxAddress{static_cast<std::uint16_t>(red + 2U)});
    }
}

void RgbWashBar::clear() noexcept {
    set_all(Rgb{});
}

void RgbWashBar::set_all(const Rgb color) noexcept {
    for (RgbWash& wash : washes_) {
        wash.set_color(color);
    }
}

void RgbWashBar::set_wash(const std::size_t index, const Rgb color) {
    if (index >= washes_.size()) {
        throw std::out_of_range{"RGB wash index is outside the bar"};
    }
    washes_.at(index).set_color(color);
}

void RgbWashBar::render_to(DmxFrame& frame) const {
    for (const RgbWash& wash : washes_) {
        wash.render_to(frame);
    }
}

Rgb RgbWashBar::wash_color(const std::size_t index) const {
    if (index >= washes_.size()) {
        throw std::out_of_range{"RGB wash index is outside the bar"};
    }
    return washes_.at(index).color();
}

}  // namespace lightengine
