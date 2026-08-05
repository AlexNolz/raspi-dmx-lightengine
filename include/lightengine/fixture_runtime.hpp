#pragma once

#include "lightengine/color.hpp"
#include "lightengine/dmx.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
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

struct RgbParLook final {
    std::uint8_t master{};
    Rgb color{};
};

class RgbPar final {
public:
    explicit RgbPar(DmxAddress start_address) : start_address_{start_address} {}

    void render_to(DmxFrame& frame, const RgbParLook& look) const;

private:
    DmxAddress start_address_;
};

struct Zkymzl11Look final {
    std::uint8_t pan{85};
    std::uint8_t tilt{179};
    std::uint8_t color_wheel{3};
    std::uint8_t gobo{};
    std::uint8_t shutter{};
    std::uint8_t dimmer{};
    std::uint8_t movement_speed{150};
    std::uint8_t reset{};
};

struct FixtureWheelSlot final {
    std::string id;
    std::string label;
    std::uint8_t value{};
};

struct Zkymzl11Profile final {
    std::vector<FixtureWheelSlot> colors;
    std::vector<FixtureWheelSlot> gobos;
    std::uint8_t color_test_min{};
    std::uint8_t color_test_max{127};
    std::uint8_t color_test_step{1};
    std::uint8_t color_test_default{8};
    double pan_min{0.14};
    double pan_max{0.54};
    double pan_center{0.333};
    double pan_width{0.30};
    double pan_direction{1.0};
    double left_pan_scale{1.0};
    double target_y_offset{};
    double tilt_min{0.56};
    double tilt_max{0.82};
    std::uint8_t reset_value{204};
    double reset_hold_seconds{6.0};

    [[nodiscard]] static Zkymzl11Profile load_from_file(const std::string& path);
    [[nodiscard]] std::optional<std::uint8_t> color_value(const std::string& id) const;
    [[nodiscard]] std::optional<std::uint8_t> gobo_value(const std::string& id) const;
};

class Zkymzl11MovingHead final {
public:
    explicit Zkymzl11MovingHead(DmxAddress start_address) : start_address_{start_address} {}

    void render_to(DmxFrame& frame, const Zkymzl11Look& look) const;

private:
    DmxAddress start_address_;
};

}  // namespace lightengine
