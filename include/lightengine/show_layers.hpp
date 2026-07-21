#pragma once

#include "lightengine/fixture_runtime.hpp"
#include "lightengine/motion_scene.hpp"
#include "lightengine/music_dynamics.hpp"
#include "lightengine/rgb_scene.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace lightengine {

struct ShowLayerContext final {
    BeatSnapshot beat;
    MusicDynamicsSnapshot dynamics;
    double mood{0.5};
    std::string_view preset;
    std::uint64_t show_seed{};
};

struct ColorLayerSelection final {
    RgbPalette palette;
    std::int64_t hold_beats{32};
    std::int64_t epoch{};
};

class ColorLayer final {
public:
    [[nodiscard]] ColorLayerSelection resolve(
        const RgbSceneMixer& mixer,
        const ShowLayerContext& context) const;
};

class SceneLayerPlanner final {
public:
    [[nodiscard]] std::string select_rgb_scene(
        const std::vector<std::string>& enabled,
        const std::vector<RgbSceneDefinition>& definitions,
        const ShowLayerContext& context,
        std::uint64_t selection_nonce = 0) const;
    [[nodiscard]] std::string select_motion_scene(
        const std::vector<std::string>& enabled,
        const MotionSceneLibrary& library,
        const ShowLayerContext& context) const;
};

struct GoboLayerRequest final {
    bool enabled{false};
    std::string_view mode{"musical"};
    std::string_view selected_gobo{"open"};
    bool highpoint_only{false};
    bool shake_enabled{false};
    double shake_mood_threshold{0.82};
    bool scene_allows_shake{false};
};

struct GoboLayerSelection final {
    std::uint8_t value{};
    std::int64_t hold_beats{16};
    bool shaking{false};
};

class GoboLayer final {
public:
    [[nodiscard]] GoboLayerSelection resolve(
        const Zkymzl11Profile& profile,
        const ShowLayerContext& context,
        const GoboLayerRequest& request,
        std::size_t fixture_index) const;
};

}  // namespace lightengine
