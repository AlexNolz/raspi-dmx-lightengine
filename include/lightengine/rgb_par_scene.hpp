#pragma once

#include "lightengine/beat_clock.hpp"
#include "lightengine/color.hpp"
#include "lightengine/rgb_scene.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace lightengine {

struct RgbParSceneDefinition final {
    std::string id;
    std::string label;
    std::string type;
    double mood_min{};
    double mood_max{1.0};
    double rate_beats{8.0};
    double dimmer_min{0.45};
    double dimmer_max{0.85};
};

struct RgbParSceneOutput final {
    std::array<Rgb, 3> colors{};
    double master_scale{1.0};
};

class RgbParSceneLibrary final {
public:
    void load_from_file(const std::string& path);

    [[nodiscard]] const std::vector<RgbParSceneDefinition>& scenes() const noexcept;
    [[nodiscard]] const RgbParSceneDefinition* find(std::string_view id) const noexcept;
    [[nodiscard]] const RgbParSceneDefinition& select_auto(
        double mood,
        const BeatSnapshot& beat,
        std::uint64_t seed) const;
    [[nodiscard]] RgbParSceneOutput evaluate(
        const RgbParSceneDefinition& scene,
        const BeatSnapshot& beat,
        double mood,
        const RgbPalette& palette) const;
    [[nodiscard]] std::string labels_json() const;

private:
    std::vector<RgbParSceneDefinition> scenes_;
};

}  // namespace lightengine
