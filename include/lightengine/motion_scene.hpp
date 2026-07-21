#pragma once

#include "lightengine/beat_clock.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace lightengine {

struct MotionPoint final {
    double x{0.5};
    double y{0.5};
};

struct MotionSceneDefinition final {
    std::string id;
    std::string label;
    std::string type;
    std::vector<MotionPoint> points;
    std::string grouping{"fixture"};
    double speed_low{0.25};
    double speed_high{0.75};
    double step_low{8.0};
    double step_high{3.0};
    double dimmer_min{0.35};
    double dimmer_max{1.0};
    double decay{5.0};
    double x_amount{0.3};
    double y_amount{0.2};
    double energy_min{0.0};
    double energy_max{1.0};
    bool allow_shake{false};
};

struct MotionTarget final {
    double x{0.5};
    double y{0.5};
    double dimmer_scale{1.0};
};

class MotionSceneLibrary final {
public:
    void load_from_file(const std::string& path);

    [[nodiscard]] const std::vector<MotionSceneDefinition>& scenes() const noexcept;
    [[nodiscard]] const MotionSceneDefinition* find(std::string_view id) const noexcept;
    [[nodiscard]] MotionTarget evaluate(
        const MotionSceneDefinition& scene,
        std::size_t fixture_index,
        std::size_t fixture_count,
        const BeatSnapshot& beat,
        double mood,
        std::uint64_t seed) const;
    [[nodiscard]] std::string labels_json() const;

private:
    std::vector<MotionSceneDefinition> scenes_;
};

}  // namespace lightengine
