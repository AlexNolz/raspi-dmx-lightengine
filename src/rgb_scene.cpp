#include "lightengine/rgb_scene.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <regex>
#include <sstream>
#include <string_view>
#include <utility>

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

Rgb add_saturating(const Rgb a, const Rgb b) {
    return Rgb{
        static_cast<std::uint8_t>(std::min(255, static_cast<int>(a.r) + static_cast<int>(b.r))),
        static_cast<std::uint8_t>(std::min(255, static_cast<int>(a.g) + static_cast<int>(b.g))),
        static_cast<std::uint8_t>(std::min(255, static_cast<int>(a.b) + static_cast<int>(b.b))),
    };
}

Rgb palette_color(const RgbPalette& palette, const std::size_t index) {
    if (palette.colors.empty()) {
        return Rgb{255, 255, 255};
    }
    return palette.colors.at(index % palette.colors.size());
}

std::optional<Rgb> named_color_rgb(const std::string& name) {
    if (name == "black") {
        return Rgb{0, 0, 0};
    }
    if (name == "white") {
        return Rgb{255, 255, 255};
    }
    if (name == "red") {
        return Rgb{255, 0, 0};
    }
    if (name == "green") {
        return Rgb{0, 255, 0};
    }
    if (name == "blue") {
        return Rgb{0, 0, 255};
    }
    if (name == "yellow") {
        return Rgb{255, 220, 0};
    }
    if (name == "cyan") {
        return Rgb{0, 220, 255};
    }
    if (name == "magenta") {
        return Rgb{255, 0, 220};
    }
    if (name == "amber") {
        return Rgb{255, 120, 0};
    }
    if (name == "orange") {
        return Rgb{255, 80, 0};
    }
    if (name == "pink") {
        return Rgb{255, 35, 130};
    }
    if (name == "teal") {
        return Rgb{0, 220, 180};
    }
    if (name == "uv") {
        return Rgb{180, 0, 255};
    }
    if (name == "acid") {
        return Rgb{180, 255, 0};
    }
    return std::nullopt;
}

std::vector<Rgb> resolve_named_colors(const std::vector<std::string>& names) {
    std::vector<Rgb> colors;
    for (const std::string& name : names) {
        if (const std::optional<Rgb> color = named_color_rgb(name)) {
            colors.push_back(*color);
        }
    }
    return colors;
}

double beat_hit(const BeatSnapshot& beat, const double sharpness) {
    return std::exp(-beat.phase * sharpness) * (0.35 + beat.strength * 0.65);
}

class PatternScene final : public RgbScene {
public:
    explicit PatternScene(std::string scene_id) : scene_id_{std::move(scene_id)} {}

    [[nodiscard]] std::string_view id() const override {
        return scene_id_;
    }

    void render(RgbWashBar& bar, const RgbSceneContext& context, const RgbPalette& palette) const override {
        const std::size_t count = std::max<std::size_t>(1U, bar.size());
        const std::size_t pair_count = std::max<std::size_t>(1U, (count + 1U) / 2U);
        const std::size_t step = static_cast<std::size_t>(std::max<std::int64_t>(0, context.beat.position));
        const double hit = beat_hit(context.beat, 9.0);
        for (std::size_t segment = 0; segment < bar.size(); ++segment) {
            const std::size_t pair_segment = segment / 2U;
            Rgb color{};
            double level = 1.0;
            if (scene_id_ == "ball") {
                const double position = (std::sin(context.beat.beat * 0.72) + 1.0) * 0.5 * static_cast<double>(pair_count - 1U);
                level = 0.06 + std::exp(-std::abs(static_cast<double>(pair_segment) - position) * 1.45) * 0.90;
                color = palette_color(palette, step / 4U);
            } else if (scene_id_ == "solid_flood") {
                color = palette_color(palette, step / 16U + pair_segment / 2U);
                level = 0.78;
            } else if (scene_id_ == "pair_swap") {
                const bool alternate = ((segment / 2U) + step / 2U) % 2U != 0U;
                color = palette_color(palette, alternate ? 1U : 0U);
                level = 0.20 + hit * (alternate ? 0.75 : 0.46);
            } else if (scene_id_ == "rainbow") {
                const double travel = static_cast<double>(pair_segment) / static_cast<double>(pair_count) + context.beat.beat * 0.035;
                const double palette_position = travel * static_cast<double>(std::max<std::size_t>(1U, palette.colors.size()));
                const auto slot = static_cast<std::size_t>(std::floor(palette_position));
                color = mix(palette_color(palette, slot), palette_color(palette, slot + 1U), palette_position - std::floor(palette_position));
                level = 0.42 + context.mood * 0.45;
            } else if (scene_id_ == "scanner") {
                const double span = static_cast<double>(std::max<std::size_t>(1U, pair_count * 2U - 2U));
                double position = std::fmod(context.beat.beat * 1.1, span);
                if (position > static_cast<double>(pair_count - 1U)) {
                    position = span - position;
                }
                level = 0.04 + std::exp(-std::abs(static_cast<double>(pair_segment) - position) * 1.7) * 0.92;
                color = palette_color(palette, step / 4U);
            } else if (scene_id_ == "sparkle") {
                const std::size_t hash = pair_segment * 37U + step * 101U;
                const bool active = hash % 11U < (context.mood > 0.65 ? 3U : 2U);
                color = palette_color(palette, hash);
                level = active ? (0.35 + hit * 0.65) : 0.025;
            } else if (scene_id_ == "split") {
                const bool left = segment < count / 2U;
                color = palette_color(palette, left ? step / 4U : step / 4U + 1U);
                level = 0.18 + hit * (left == (step % 2U == 0U) ? 0.82 : 0.34);
            } else if (scene_id_ == "blocks") {
                color = palette_color(palette, pair_segment + step / 4U);
                level = ((pair_segment + step) % 3U == 0U) ? 0.95 : 0.08;
            } else if (scene_id_ == "strobe") {
                color = palette_color(palette, step);
                level = context.beat.phase < (0.08 + context.mood * 0.12) ? 1.0 : 0.0;
            } else if (scene_id_ == "quad_strobe") {
                const bool left_half = segment < count / 2U;
                const bool left_on = step % 2U == 0U;
                color = palette_color(palette, left_half ? step : step + 1U);
                level = context.beat.phase < 0.20 && left_half == left_on ? 1.0 : 0.0;
            } else if (scene_id_ == "breathe") {
                color = mix(palette_color(palette, 0), palette_color(palette, 1), static_cast<double>(pair_segment) / static_cast<double>(pair_count));
                level = 0.10 + (std::sin(context.beat.beat * 0.32) * 0.5 + 0.5) * 0.42;
            } else if (scene_id_ == "theater") {
                color = palette_color(palette, pair_segment + step / 4U);
                level = (pair_segment + step) % 3U == 0U ? 0.92 : 0.055;
            } else if (scene_id_ == "zipper") {
                const std::size_t distance = std::min(pair_segment, pair_count - 1U - pair_segment);
                color = palette_color(palette, distance + step / 4U);
                level = distance == (step % ((pair_count + 1U) / 2U)) ? 0.95 : 0.06;
            } else if (scene_id_ == "orbit") {
                const double angle = context.beat.beat * 0.62 + static_cast<double>(pair_segment) * 6.283185307 / static_cast<double>(pair_count);
                color = mix(palette_color(palette, 0), palette_color(palette, 2), std::sin(angle) * 0.5 + 0.5);
                level = 0.12 + (std::cos(angle) * 0.5 + 0.5) * 0.73;
            } else if (scene_id_ == "traffic") {
                color = palette_color(palette, pair_segment % 3U);
                level = pair_segment % 3U == step % 3U ? 0.90 : 0.07;
            } else if (scene_id_ == "gate") {
                const bool open = context.beat.phase < (0.20 + context.mood * 0.25);
                color = palette_color(palette, pair_segment + step / 2U);
                level = open && (pair_segment + step) % 2U == 0U ? 0.95 : 0.035;
            } else if (scene_id_ == "binary") {
                color = palette_color(palette, pair_segment);
                level = ((step >> (pair_segment % 6U)) & 1U) != 0U ? 0.88 : 0.035;
            } else if (scene_id_ == "fill") {
                const std::size_t filled = step % (pair_count + 1U);
                color = palette_color(palette, step / (pair_count + 1U));
                level = pair_segment < filled ? 0.82 : 0.035;
            } else if (scene_id_ == "siren") {
                const bool left = segment < count / 2U;
                const bool phase_left = static_cast<std::size_t>(std::floor(context.beat.beat * 2.0)) % 2U == 0U;
                color = palette_color(palette, left ? 0U : 1U);
                level = left == phase_left ? 0.95 : 0.025;
            }
            bar.set_wash(segment, scale(color, context.master * level));
        }
    }

private:
    std::string scene_id_;
};

class StaticGlowScene final : public RgbScene {
public:
    [[nodiscard]] std::string_view id() const override {
        return "static_glow";
    }

    void render(RgbWashBar& bar, const RgbSceneContext& context, const RgbPalette& palette) const override {
        for (std::size_t segment = 0; segment < bar.size(); ++segment) {
            const std::size_t group = segment / 2U;
            const Rgb base = mix(palette_color(palette, 0), palette_color(palette, 1), static_cast<double>(group) / 3.0);
            const double shimmer = std::sin(context.beat.beat * 0.35 + static_cast<double>(group) * 0.75) * 0.5 + 0.5;
            bar.set_wash(segment, scale(base, context.master * (0.16 + shimmer * 0.12 + context.mood * 0.18)));
        }
    }
};

class BeatPulseScene final : public RgbScene {
public:
    [[nodiscard]] std::string_view id() const override {
        return "beat_pulse";
    }

    void render(RgbWashBar& bar, const RgbSceneContext& context, const RgbPalette& palette) const override {
        const double hit = beat_hit(context.beat, 8.5);
        for (std::size_t segment = 0; segment < bar.size(); ++segment) {
            const std::size_t group = segment / 2U;
            const double distance = std::abs(static_cast<double>(group) - 1.5) / 1.5;
            const double center = 1.0 - clamp01(distance);
            const Rgb base = mix(palette_color(palette, 1), palette_color(palette, 2), center);
            bar.set_wash(segment, scale(base, context.master * (0.06 + hit * (0.45 + center * 0.35))));
        }
    }
};

class ChaseScene final : public RgbScene {
public:
    [[nodiscard]] std::string_view id() const override {
        return "chase";
    }

    void render(RgbWashBar& bar, const RgbSceneContext& context, const RgbPalette& palette) const override {
        for (std::size_t segment = 0; segment < bar.size(); ++segment) {
            const std::size_t group = segment / 2U;
            const double wave = std::sin(context.beat.beat * 1.25 - static_cast<double>(group) * 1.10) * 0.5 + 0.5;
            const double gate = std::pow(wave, 2.8);
            bar.set_wash(segment, scale(palette_color(palette, group + 2U), context.master * (0.03 + gate * 0.70)));
        }
    }
};

class CometScene final : public RgbScene {
public:
    [[nodiscard]] std::string_view id() const override {
        return "comet";
    }

    void render(RgbWashBar& bar, const RgbSceneContext& context, const RgbPalette& palette) const override {
        const std::size_t group_count = std::max<std::size_t>(1U, (bar.size() + 1U) / 2U);
        const double head = std::fmod(context.beat.beat * 1.3, static_cast<double>(group_count));
        for (std::size_t segment = 0; segment < bar.size(); ++segment) {
            const std::size_t group = segment / 2U;
            const double raw_distance = std::abs(static_cast<double>(group) - head);
            const double wrapped_distance = std::min(raw_distance, static_cast<double>(group_count) - raw_distance);
            const double tail = std::exp(-wrapped_distance * 1.35);
            bar.set_wash(segment, scale(palette_color(palette, static_cast<std::size_t>(context.beat.position / 4) + 3U), context.master * tail * 0.75));
        }
    }
};

class BeatSparkScene final : public RgbScene {
public:
    [[nodiscard]] std::string_view id() const override {
        return "beat_spark";
    }

    void render(RgbWashBar& bar, const RgbSceneContext& context, const RgbPalette& palette) const override {
        const double hit = beat_hit(context.beat, 14.0);
        const auto seed = static_cast<std::size_t>(context.beat.position);
        for (std::size_t segment = 0; segment < bar.size(); ++segment) {
            const std::size_t group = segment / 2U;
            const bool active = ((group * 5U + seed * 3U) % 8U) < 2U;
            if (active) {
                bar.set_wash(segment, scale(palette_color(palette, seed + group), context.master * hit * 0.95));
            }
        }
    }
};

std::optional<std::string> regex_string_field(const std::string& object, const std::string& field) {
    const std::regex pattern{"\"" + field + "\"\\s*:\\s*\"([^\"]*)\""};
    std::smatch match;
    if (std::regex_search(object, match, pattern)) {
        return match.str(1);
    }
    return std::nullopt;
}

std::optional<double> regex_number_field(const std::string& object, const std::string& field) {
    const std::regex pattern{"\"" + field + R"("\s*:\s*(-?[0-9]+(?:\.[0-9]+)?))"};
    std::smatch match;
    if (!std::regex_search(object, match, pattern)) {
        return std::nullopt;
    }
    char* end = nullptr;
    const double value = std::strtod(match.str(1).c_str(), &end);
    if (end == match.str(1).c_str()) {
        return std::nullopt;
    }
    return value;
}

std::vector<std::string> regex_string_list_field(const std::string& object, const std::string& field) {
    const std::string key = "\"" + field + "\"";
    const std::size_t key_position = object.find(key);
    if (key_position == std::string::npos) {
        return {};
    }
    const std::size_t array_start = object.find('[', key_position + key.size());
    const std::size_t array_end = array_start == std::string::npos ? std::string::npos : object.find(']', array_start + 1U);
    if (array_start == std::string::npos || array_end == std::string::npos) {
        return {};
    }

    std::vector<std::string> values;
    const std::string list = object.substr(array_start + 1U, array_end - array_start - 1U);
    const std::regex string_pattern{"\"([^\"]+)\""};
    for (auto current = std::sregex_iterator{list.begin(), list.end(), string_pattern}; current != std::sregex_iterator{}; ++current) {
        values.push_back(current->str(1));
    }
    return values;
}

std::vector<std::string> json_objects_with_ids(const std::string& text) {
    std::vector<std::string> objects;
    std::vector<std::size_t> starts;
    bool in_string = false;
    bool escaped = false;
    for (std::size_t index = 0; index < text.size(); ++index) {
        const char character = text.at(index);
        if (in_string) {
            if (escaped) {
                escaped = false;
            } else if (character == '\\') {
                escaped = true;
            } else if (character == '"') {
                in_string = false;
            }
            continue;
        }
        if (character == '"') {
            in_string = true;
        } else if (character == '{') {
            starts.push_back(index);
        } else if (character == '}' && !starts.empty()) {
            const std::size_t start = starts.back();
            starts.pop_back();
            std::string object = text.substr(start, index - start + 1U);
            if (object.find('{', 1U) == std::string::npos && regex_string_field(object, "id")) {
                objects.push_back(std::move(object));
            }
        }
    }
    return objects;
}

std::vector<Rgb> regex_color_list_field(const std::string& object, const std::string& field) {
    const std::string key = "\"" + field + "\"";
    const std::size_t key_position = object.find(key);
    if (key_position == std::string::npos) {
        return {};
    }

    const std::size_t array_start = object.find('[', key_position + key.size());
    if (array_start == std::string::npos) {
        return {};
    }

    int depth = 0;
    std::size_t array_end = std::string::npos;
    for (std::size_t index = array_start; index < object.size(); ++index) {
        if (object.at(index) == '[') {
            ++depth;
        } else if (object.at(index) == ']') {
            --depth;
            if (depth == 0) {
                array_end = index;
                break;
            }
        }
    }
    if (array_end == std::string::npos) {
        return {};
    }

    std::vector<Rgb> colors;
    const std::string list = object.substr(array_start, array_end - array_start + 1U);
    const std::regex color_pattern{R"(\[\s*([0-9]{1,3})\s*,\s*([0-9]{1,3})\s*,\s*([0-9]{1,3})\s*\])"};
    for (auto current = std::sregex_iterator{list.begin(), list.end(), color_pattern}; current != std::sregex_iterator{}; ++current) {
        colors.push_back(Rgb{
            static_cast<std::uint8_t>(std::min(255, std::stoi(current->str(1)))),
            static_cast<std::uint8_t>(std::min(255, std::stoi(current->str(2)))),
            static_cast<std::uint8_t>(std::min(255, std::stoi(current->str(3)))),
        });
    }
    return colors;
}

double clamp_range(const double value, const double low, const double high) {
    return std::max(low, std::min(high, value));
}

std::vector<RgbSceneDefinition> default_scene_definitions() {
    return {
        {"rgb_static", "Static Glow", "static_glow", "preset", 0.65, 0.55},
        {"rgb_beat_pulse", "Beat Pulse", "beat_pulse", "preset", 1.0, 0.85},
        {"rgb_chase", "Chase", "chase", "preset", 1.0, 0.7},
        {"rgb_comet", "Comet", "comet", "preset", 1.0, 0.75},
        {"rgb_spark", "Beat Spark", "beat_spark", "preset", 1.0, 0.85},
        {"rgb_amber_glow", "Amber Glow", "static_glow", "amber_warm", 0.45, 0.5},
    };
}

}  // namespace

RgbSceneMixer::RgbSceneMixer()
    : palettes_{
          {"club_blue_amber", "Club Blue / Cyan / Amber", {"blue", "cyan", "amber", "pink"}, resolve_named_colors({"blue", "cyan", "amber", "pink"})},
          {"club_teal_pink", "Club Teal / Pink", {"teal", "pink", "blue", "yellow"}, resolve_named_colors({"teal", "pink", "blue", "yellow"})},
          {"rave_neon", "Rave Neon", {"green", "magenta", "cyan", "yellow"}, resolve_named_colors({"green", "magenta", "cyan", "yellow"})},
          {"rave_acid", "Rave Acid", {"acid", "cyan", "red", "uv"}, resolve_named_colors({"acid", "cyan", "red", "uv"})},
          {"rgb_hard", "Hard RGB", {"red", "blue", "green"}, resolve_named_colors({"red", "blue", "green"})},
          {"deep_blue", "Deep Blue", {"blue", "cyan", "uv", "teal"}, resolve_named_colors({"blue", "cyan", "uv", "teal"})},
          {"amber_warm", "Warm Amber", {"orange", "amber", "red", "yellow"}, resolve_named_colors({"orange", "amber", "red", "yellow"})},
      },
      preset_palette_sets_{
          {"warmup", {"amber_warm"}},
          {"club", {"club_blue_amber", "club_teal_pink", "deep_blue"}},
          {"rave", {"rave_neon", "rave_acid", "rgb_hard"}},
          {"game_show", {"deep_blue", "club_blue_amber"}},
          {"rgb_hard", {"rgb_hard"}},
      },
      scene_definitions_{default_scene_definitions()} {
    scenes_.push_back(std::make_unique<StaticGlowScene>());
    scenes_.push_back(std::make_unique<BeatPulseScene>());
    scenes_.push_back(std::make_unique<ChaseScene>());
    scenes_.push_back(std::make_unique<CometScene>());
    scenes_.push_back(std::make_unique<BeatSparkScene>());
    for (const char* pattern : {"ball", "solid_flood", "pair_swap", "rainbow", "scanner", "sparkle", "split", "blocks", "strobe", "quad_strobe",
             "breathe", "theater", "zipper", "orbit", "traffic", "gate", "binary", "fill", "siren"}) {
        scenes_.push_back(std::make_unique<PatternScene>(pattern));
    }
}

const std::vector<std::unique_ptr<RgbScene>>& RgbSceneMixer::scenes() const {
    return scenes_;
}

const std::vector<RgbPalette>& RgbSceneMixer::palettes() const {
    return palettes_;
}

const std::vector<RgbSceneDefinition>& RgbSceneMixer::scene_definitions() const {
    return scene_definitions_;
}

const RgbPalette& RgbSceneMixer::palette_for_preset(const std::string_view preset) const {
    return palette_for_preset(preset, 0);
}

const RgbPalette& RgbSceneMixer::palette_for_preset(const std::string_view preset, const std::int64_t seed) const {
    const auto found = std::find_if(preset_palette_sets_.begin(), preset_palette_sets_.end(), [&](const PresetPaletteSet& set) {
        return set.preset_id == preset;
    });
    if (found == preset_palette_sets_.end() || found->palette_ids.empty()) {
        return palette_by_id("club_blue_amber");
    }

    const auto index = static_cast<std::size_t>(std::abs(seed)) % found->palette_ids.size();
    return palette_by_id(found->palette_ids.at(index));
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

std::vector<std::string> RgbSceneMixer::palette_ids_for_preset(const std::string_view preset) const {
    const auto found = std::find_if(preset_palette_sets_.begin(), preset_palette_sets_.end(), [&](const PresetPaletteSet& set) {
        return set.preset_id == preset;
    });
    if (found != preset_palette_sets_.end() && !found->palette_ids.empty()) {
        return found->palette_ids;
    }
    return {palette_for_preset(preset).id};
}

void RgbSceneMixer::load_palettes_from_file(const std::string& path) {
    std::ifstream file{path};
    if (!file) {
        return;
    }

    const std::string text{std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{}};
    std::vector<RgbPalette> loaded_palettes;
    std::vector<PresetPaletteSet> loaded_sets;
    for (const std::string& object : json_objects_with_ids(text)) {
        const std::string id = regex_string_field(object, "id").value_or("");
        if (id.empty()) {
            continue;
        }

        std::vector<Rgb> colors = regex_color_list_field(object, "colors");
        if (!colors.empty()) {
            loaded_palettes.push_back(RgbPalette{id, regex_string_field(object, "name").value_or(id), {}, std::move(colors)});
            continue;
        }

        std::vector<std::string> color_names = regex_string_list_field(object, "colors");
        colors = resolve_named_colors(color_names);
        if (!colors.empty()) {
            loaded_palettes.push_back(RgbPalette{id, regex_string_field(object, "name").value_or(id), std::move(color_names), std::move(colors)});
            continue;
        }

        std::vector<std::string> palette_refs = regex_string_list_field(object, "palettes");
        if (!palette_refs.empty()) {
            loaded_sets.push_back(PresetPaletteSet{id, std::move(palette_refs)});
        }
    }

    if (!loaded_palettes.empty()) {
        palettes_ = std::move(loaded_palettes);
    }
    if (!loaded_sets.empty()) {
        preset_palette_sets_ = std::move(loaded_sets);
    }
}

std::string RgbSceneMixer::effects_json() const {
    std::ostringstream out;
    out << '{';
    bool first = true;
    for (const RgbSceneDefinition& definition : scene_definitions_) {
        if (definition.id.starts_with("standby_") || definition.id.starts_with("rgb_")) {
            continue;
        }
        if (!first) {
            out << ',';
        }
        first = false;
        out << '"' << definition.id << R"(":")" << definition.label << '"';
    }
    out << '}';
    return out.str();
}

std::string RgbSceneMixer::palettes_json() const {
    std::ostringstream out;
    out << '{';
    for (std::size_t index = 0; index < palettes_.size(); ++index) {
        if (index != 0U) {
            out << ',';
        }
        const RgbPalette& palette = palettes_.at(index);
        out << '"' << palette.id << R"(":{"name":")" << palette.label << R"(","colors":)";
        out << '[';
        for (std::size_t color_index = 0; color_index < palette.color_names.size(); ++color_index) {
            if (color_index != 0U) {
                out << ',';
            }
            out << '"' << palette.color_names.at(color_index) << '"';
        }
        out << "]}";
    }
    out << '}';
    return out.str();
}

void RgbSceneMixer::load_scene_definitions_from_file(const std::string& path) {
    std::ifstream file{path};
    if (!file) {
        return;
    }

    const std::string text{std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{}};
    const std::regex object_pattern{R"(\{[^{}]*"id"\s*:\s*"[^"]+"[^{}]*\})"};
    std::vector<RgbSceneDefinition> loaded;
    for (auto current = std::sregex_iterator{text.begin(), text.end(), object_pattern}; current != std::sregex_iterator{}; ++current) {
        const std::string object = current->str();
        RgbSceneDefinition definition;
        definition.id = regex_string_field(object, "id").value_or("");
        definition.label = regex_string_field(object, "name").value_or(definition.id);
        definition.type = regex_string_field(object, "type").value_or("");
        definition.palette = regex_string_field(object, "palette").value_or("club");
        definition.speed = clamp_range(regex_number_field(object, "speed").value_or(1.0), 0.05, 8.0);
        definition.intensity = clamp_range(regex_number_field(object, "intensity").value_or(1.0), 0.0, 1.0);
        definition.energy_min = clamp_range(regex_number_field(object, "energy_min").value_or(0.0), 0.0, 1.0);
        definition.energy_max = clamp_range(regex_number_field(object, "energy_max").value_or(1.0), 0.0, 1.0);
        definition.color_slots = static_cast<std::size_t>(clamp_range(
            regex_number_field(object, "color_slots").value_or(4.0), 1.0, 8.0));
        if (!definition.id.empty() && !definition.type.empty()) {
            loaded.push_back(std::move(definition));
        }
    }

    if (!loaded.empty()) {
        scene_definitions_ = std::move(loaded);
    }
}

void RgbSceneMixer::render(
    RgbWashBar& bar,
    const std::vector<std::string>& active_scene_ids,
    const RgbSceneContext& context,
    const RgbPalette& palette) const {
    for (const std::string& definition_id : active_scene_ids) {
        const auto definition = std::find_if(scene_definitions_.begin(), scene_definitions_.end(), [&](const RgbSceneDefinition& item) {
            return item.id == definition_id;
        });
        if (definition == scene_definitions_.end()) {
            continue;
        }

        const auto renderer = std::find_if(scenes_.begin(), scenes_.end(), [&](const std::unique_ptr<RgbScene>& scene) {
            return scene->id() == definition->type;
        });
        if (renderer != scenes_.end()) {
            RgbSceneContext scene_context = context;
            scene_context.master *= definition->intensity;
            scene_context.beat.beat *= definition->speed;
            const RgbPalette& resolved_palette = definition->palette.empty() || definition->palette == "preset"
                ? palette
                : palette_by_id(definition->palette);
            RgbPalette scene_palette = resolved_palette;
            if (scene_palette.colors.size() > definition->color_slots) {
                scene_palette.colors.resize(definition->color_slots);
            }
            if (scene_palette.color_names.size() > definition->color_slots) {
                scene_palette.color_names.resize(definition->color_slots);
            }
            RgbWashBar layer{DmxAddress{1}, static_cast<std::uint8_t>(bar.size())};
            (*renderer)->render(layer, scene_context, scene_palette);
            for (std::size_t segment = 0; segment < bar.size(); ++segment) {
                bar.set_wash(segment, add_saturating(bar.wash_color(segment), layer.wash_color(segment)));
            }
        }
    }
}

}  // namespace lightengine
