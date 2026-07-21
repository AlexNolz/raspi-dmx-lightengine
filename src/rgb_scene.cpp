#include "lightengine/rgb_scene.hpp"

#include <algorithm>
#include <cmath>
#include <string_view>

namespace lightengine {

namespace {

double clamp01(const double value) {
    return std::max(0.0, std::min(1.0, value));
}

std::uint8_t to_dmx(const double value) {
    return static_cast<std::uint8_t>(std::lround(clamp01(value) * 255.0));
}

Rgb scale(const Rgb color, const double level) {
    const double clamped = clamp01(level);
    return Rgb{
        to_dmx((static_cast<double>(color.r) / 255.0) * clamped),
        to_dmx((static_cast<double>(color.g) / 255.0) * clamped),
        to_dmx((static_cast<double>(color.b) / 255.0) * clamped),
    };
}

Rgb mix(const Rgb a, const Rgb b, const double amount) {
    const double t = clamp01(amount);
    const double inv = 1.0 - t;
    return Rgb{
        to_dmx((static_cast<double>(a.r) * inv + static_cast<double>(b.r) * t) / 255.0),
        to_dmx((static_cast<double>(a.g) * inv + static_cast<double>(b.g) * t) / 255.0),
        to_dmx((static_cast<double>(a.b) * inv + static_cast<double>(b.b) * t) / 255.0),
    };
}

Rgb palette_color(const RgbPalette& palette, const std::size_t index) {
    if (palette.colors.empty()) {
        return Rgb{255, 255, 255};
    }
    return palette.colors.at(index % palette.colors.size());
}

double beat_hit(const BeatSnapshot& beat, const double sharpness) {
    return std::exp(-beat.phase * sharpness) * (0.35 + beat.strength * 0.65);
}

class StaticGlowScene final : public RgbScene {
public:
    [[nodiscard]] std::string_view id() const override {
        return "rgb_static";
    }

    void render(RgbWashBar& bar, const RgbSceneContext& context, const RgbPalette& palette) const override {
        for (std::size_t segment = 0; segment < bar.size(); ++segment) {
            const Rgb base = mix(palette_color(palette, 0), palette_color(palette, 1), static_cast<double>(segment) / 7.0);
            const double shimmer = std::sin(context.beat.beat * 0.35 + static_cast<double>(segment) * 0.55) * 0.5 + 0.5;
            bar.set_wash(segment, scale(base, context.master * (0.16 + shimmer * 0.12 + context.mood * 0.18)));
        }
    }
};

class BeatPulseScene final : public RgbScene {
public:
    [[nodiscard]] std::string_view id() const override {
        return "rgb_beat_pulse";
    }

    void render(RgbWashBar& bar, const RgbSceneContext& context, const RgbPalette& palette) const override {
        const double hit = beat_hit(context.beat, 8.5);
        for (std::size_t segment = 0; segment < bar.size(); ++segment) {
            const double distance = std::abs(static_cast<double>(segment) - 3.5) / 3.5;
            const double center = 1.0 - clamp01(distance);
            const Rgb base = mix(palette_color(palette, 0), palette_color(palette, 2), center);
            bar.set_wash(segment, scale(base, context.master * (0.06 + hit * (0.45 + center * 0.35))));
        }
    }
};

class ChaseScene final : public RgbScene {
public:
    [[nodiscard]] std::string_view id() const override {
        return "rgb_chase";
    }

    void render(RgbWashBar& bar, const RgbSceneContext& context, const RgbPalette& palette) const override {
        for (std::size_t segment = 0; segment < bar.size(); ++segment) {
            const double wave = std::sin(context.beat.beat * 1.25 - static_cast<double>(segment) * 0.85) * 0.5 + 0.5;
            const double gate = std::pow(wave, 2.8);
            bar.set_wash(segment, scale(palette_color(palette, segment), context.master * (0.03 + gate * 0.58)));
        }
    }
};

class CometScene final : public RgbScene {
public:
    [[nodiscard]] std::string_view id() const override {
        return "rgb_comet";
    }

    void render(RgbWashBar& bar, const RgbSceneContext& context, const RgbPalette& palette) const override {
        const double head = std::fmod(context.beat.beat * 1.6, static_cast<double>(bar.size()));
        for (std::size_t segment = 0; segment < bar.size(); ++segment) {
            const double raw_distance = std::abs(static_cast<double>(segment) - head);
            const double wrapped_distance = std::min(raw_distance, static_cast<double>(bar.size()) - raw_distance);
            const double tail = std::exp(-wrapped_distance * 1.35);
            bar.set_wash(segment, scale(palette_color(palette, static_cast<std::size_t>(context.beat.position / 4)), context.master * tail * 0.75));
        }
    }
};

class BeatSparkScene final : public RgbScene {
public:
    [[nodiscard]] std::string_view id() const override {
        return "rgb_spark";
    }

    void render(RgbWashBar& bar, const RgbSceneContext& context, const RgbPalette& palette) const override {
        const double hit = beat_hit(context.beat, 14.0);
        const auto seed = static_cast<std::size_t>(context.beat.position);
        for (std::size_t segment = 0; segment < bar.size(); ++segment) {
            const bool active = ((segment * 5U + seed * 3U) % 8U) < 2U;
            if (active) {
                bar.set_wash(segment, scale(palette_color(palette, seed + segment), context.master * hit * 0.9));
            }
        }
    }
};

}  // namespace

RgbSceneMixer::RgbSceneMixer()
    : palettes_{
          {"club", "Club Magenta/Blue", {Rgb{255, 0, 130}, Rgb{40, 0, 255}, Rgb{0, 210, 255}, Rgb{190, 0, 255}}},
          {"rave", "Rave Neon", {Rgb{0, 255, 80}, Rgb{255, 0, 220}, Rgb{0, 170, 255}, Rgb{255, 230, 0}}},
          {"rgb_hard", "Hard RGB", {Rgb{255, 0, 0}, Rgb{0, 255, 0}, Rgb{0, 0, 255}, Rgb{255, 255, 255}}},
          {"deep_blue", "Deep Blue", {Rgb{0, 20, 120}, Rgb{0, 120, 255}, Rgb{80, 0, 180}, Rgb{0, 255, 200}}},
          {"amber", "Warm Amber", {Rgb{255, 80, 0}, Rgb{255, 150, 20}, Rgb{255, 35, 10}, Rgb{255, 220, 90}}},
      } {
    scenes_.push_back(std::make_unique<StaticGlowScene>());
    scenes_.push_back(std::make_unique<BeatPulseScene>());
    scenes_.push_back(std::make_unique<ChaseScene>());
    scenes_.push_back(std::make_unique<CometScene>());
    scenes_.push_back(std::make_unique<BeatSparkScene>());
}

const std::vector<std::unique_ptr<RgbScene>>& RgbSceneMixer::scenes() const {
    return scenes_;
}

const std::vector<RgbPalette>& RgbSceneMixer::palettes() const {
    return palettes_;
}

const RgbPalette& RgbSceneMixer::palette_for_preset(const std::string_view preset) const {
    if (preset == "rave") {
        return palette_by_id("rave");
    }
    if (preset == "rgb_hard") {
        return palette_by_id("rgb_hard");
    }
    return palette_by_id("club");
}

const RgbPalette& RgbSceneMixer::palette_by_id(const std::string_view id) const {
    const auto found = std::find_if(palettes_.begin(), palettes_.end(), [&](const RgbPalette& palette) {
        return palette.id == id;
    });
    if (found != palettes_.end()) {
        return *found;
    }
    return palettes_.front();
}

void RgbSceneMixer::render(
    RgbWashBar& bar,
    const std::vector<std::string>& active_scene_ids,
    const RgbSceneContext& context,
    const RgbPalette& palette) const {
    for (const std::string& scene_id : active_scene_ids) {
        const auto found = std::find_if(scenes_.begin(), scenes_.end(), [&](const std::unique_ptr<RgbScene>& scene) {
            return scene->id() == scene_id;
        });
        if (found != scenes_.end()) {
            (*found)->render(bar, context, palette);
        }
    }
}

}  // namespace lightengine
