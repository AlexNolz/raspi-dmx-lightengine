#pragma once

#include "lightengine/beat_clock.hpp"
#include "lightengine/color.hpp"
#include "lightengine/fixture_runtime.hpp"

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace lightengine {

struct RgbPalette final {
    std::string id;
    std::string label;
    std::vector<std::string> color_names;
    std::vector<Rgb> colors;
};

struct PresetPaletteSet final {
    std::string preset_id;
    std::vector<std::string> palette_ids;
};

struct RgbSceneContext final {
    BeatSnapshot beat;
    double master{1.0};
    double mood{0.5};
};

struct RgbSceneDefinition final {
    std::string id;
    std::string label;
    std::string type;
    std::string palette;
    double speed{1.0};
    double intensity{1.0};
    double energy_min{0.0};
    double energy_max{1.0};
    std::size_t color_slots{4};
};

class RgbScene {
public:
    virtual ~RgbScene() = default;

    [[nodiscard]] virtual std::string_view id() const = 0;
    virtual void render(RgbWashBar& bar, const RgbSceneContext& context, const RgbPalette& palette) const = 0;
};

class RgbSceneMixer final {
public:
    RgbSceneMixer();

    [[nodiscard]] const std::vector<std::unique_ptr<RgbScene>>& scenes() const;
    [[nodiscard]] const std::vector<RgbPalette>& palettes() const;
    [[nodiscard]] const std::vector<RgbSceneDefinition>& scene_definitions() const;
    [[nodiscard]] const RgbPalette& palette_for_preset(std::string_view preset) const;
    [[nodiscard]] const RgbPalette& palette_for_preset(std::string_view preset, std::int64_t seed) const;
    [[nodiscard]] const RgbPalette& palette_by_id(std::string_view id) const;
    [[nodiscard]] std::string effects_json() const;

    void load_palettes_from_file(const std::string& path);
    void load_scene_definitions_from_file(const std::string& path);

    void render(
        RgbWashBar& bar,
        const std::vector<std::string>& active_scene_ids,
        const RgbSceneContext& context,
        const RgbPalette& palette) const;

private:
    std::vector<std::unique_ptr<RgbScene>> scenes_;
    std::vector<RgbPalette> palettes_;
    std::vector<PresetPaletteSet> preset_palette_sets_;
    std::vector<RgbSceneDefinition> scene_definitions_;
};

}  // namespace lightengine
