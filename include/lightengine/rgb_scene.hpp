#pragma once

#include "lightengine/beat_clock.hpp"
#include "lightengine/color.hpp"
#include "lightengine/fixture_runtime.hpp"

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace lightengine {

struct RgbPalette final {
    std::string id;
    std::string label;
    std::vector<Rgb> colors;
};

struct RgbSceneContext final {
    BeatSnapshot beat;
    double master{1.0};
    double mood{0.5};
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
    [[nodiscard]] const RgbPalette& palette_for_preset(std::string_view preset) const;
    [[nodiscard]] const RgbPalette& palette_by_id(std::string_view id) const;

    void render(
        RgbWashBar& bar,
        const std::vector<std::string>& active_scene_ids,
        const RgbSceneContext& context,
        const RgbPalette& palette) const;

private:
    std::vector<std::unique_ptr<RgbScene>> scenes_;
    std::vector<RgbPalette> palettes_;
};

}  // namespace lightengine
