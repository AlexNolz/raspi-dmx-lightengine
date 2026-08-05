#include "lightengine/rgb_par_scene.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iterator>
#include <regex>
#include <sstream>
#include <stdexcept>

namespace lightengine {

namespace {

double clamp01(const double value) {
    return std::clamp(value, 0.0, 1.0);
}

double lerp(const double low, const double high, const double amount) {
    return low + (high - low) * clamp01(amount);
}

std::string string_field(const std::string& object, const std::string& key, const std::string& fallback = {}) {
    const std::regex pattern{'"' + key + R"json("\s*:\s*"([^"]*)")json"};
    std::smatch match;
    return std::regex_search(object, match, pattern) ? match[1].str() : fallback;
}

double number_field(const std::string& object, const std::string& key, const double fallback) {
    const std::regex pattern{'"' + key + R"json("\s*:\s*(-?[0-9]+(?:\.[0-9]+)?))json"};
    std::smatch match;
    return std::regex_search(object, match, pattern) ? std::stod(match[1].str()) : fallback;
}

std::vector<std::string> scene_objects(const std::string& text) {
    const std::size_t scenes_key = text.find("\"scenes\"");
    const std::size_t array_open = scenes_key == std::string::npos ? std::string::npos : text.find('[', scenes_key);
    if (array_open == std::string::npos) {
        return {};
    }
    std::vector<std::string> objects;
    int depth = 0;
    bool in_string = false;
    bool escaped = false;
    std::size_t object_start = std::string::npos;
    for (std::size_t index = array_open + 1U; index < text.size(); ++index) {
        const char current = text.at(index);
        if (in_string) {
            if (escaped) {
                escaped = false;
            } else if (current == '\\') {
                escaped = true;
            } else if (current == '"') {
                in_string = false;
            }
            continue;
        }
        if (current == '"') {
            in_string = true;
        } else if (current == '{') {
            if (depth++ == 0) {
                object_start = index;
            }
        } else if (current == '}' && depth > 0 && --depth == 0 && object_start != std::string::npos) {
            objects.push_back(text.substr(object_start, index - object_start + 1U));
            object_start = std::string::npos;
        } else if (current == ']' && depth == 0) {
            break;
        }
    }
    return objects;
}

std::uint64_t mix_seed(std::uint64_t value) {
    value ^= value >> 30U;
    value *= 0xbf58476d1ce4e5b9ULL;
    value ^= value >> 27U;
    value *= 0x94d049bb133111ebULL;
    return value ^ (value >> 31U);
}

Rgb palette_color(const RgbPalette& palette, const std::size_t index) {
    return palette.colors.empty() ? Rgb{255, 255, 255} : palette.colors.at(index % palette.colors.size());
}

Rgb scaled(const Rgb color, const double level) {
    return Rgb{
        static_cast<std::uint8_t>(std::lround(static_cast<double>(color.r) * clamp01(level))),
        static_cast<std::uint8_t>(std::lround(static_cast<double>(color.g) * clamp01(level))),
        static_cast<std::uint8_t>(std::lround(static_cast<double>(color.b) * clamp01(level))),
    };
}

}  // namespace

void RgbParSceneLibrary::load_from_file(const std::string& path) {
    std::ifstream file{path};
    if (!file) {
        throw std::runtime_error{"cannot open RGB PAR scenes: " + path};
    }
    const std::string text{std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{}};
    std::vector<RgbParSceneDefinition> loaded;
    for (const std::string& object : scene_objects(text)) {
        RgbParSceneDefinition scene;
        scene.id = string_field(object, "id");
        scene.label = string_field(object, "name", scene.id);
        scene.type = string_field(object, "type");
        scene.mood_min = clamp01(number_field(object, "mood_min", 0.0));
        scene.mood_max = clamp01(number_field(object, "mood_max", 1.0));
        scene.rate_beats = std::max(1.0, number_field(object, "rate_beats", 8.0));
        scene.dimmer_min = clamp01(number_field(object, "dimmer_min", 0.45));
        scene.dimmer_max = clamp01(number_field(object, "dimmer_max", 0.85));
        if (!scene.id.empty() && !scene.type.empty() && scene.mood_min <= scene.mood_max) {
            loaded.push_back(std::move(scene));
        }
    }
    if (loaded.empty()) {
        throw std::runtime_error{"RGB PAR scene file contains no scenes"};
    }
    scenes_ = std::move(loaded);
}

const std::vector<RgbParSceneDefinition>& RgbParSceneLibrary::scenes() const noexcept {
    return scenes_;
}

const RgbParSceneDefinition* RgbParSceneLibrary::find(const std::string_view id) const noexcept {
    const auto item = std::find_if(scenes_.begin(), scenes_.end(), [&](const RgbParSceneDefinition& scene) {
        return scene.id == id;
    });
    return item == scenes_.end() ? nullptr : &*item;
}

const RgbParSceneDefinition& RgbParSceneLibrary::select_auto(
    const double mood,
    const BeatSnapshot& beat,
    const std::uint64_t seed) const {
    std::vector<const RgbParSceneDefinition*> candidates;
    for (const RgbParSceneDefinition& scene : scenes_) {
        if (mood >= scene.mood_min && mood <= scene.mood_max) {
            candidates.push_back(&scene);
        }
    }
    if (candidates.empty()) {
        return scenes_.front();
    }
    const auto phrase = static_cast<std::uint64_t>(std::max(0.0, std::floor(beat.beat / 32.0)));
    return *candidates.at(static_cast<std::size_t>(mix_seed(seed + phrase * 131U) % candidates.size()));
}

RgbParSceneOutput RgbParSceneLibrary::evaluate(
    const RgbParSceneDefinition& scene,
    const BeatSnapshot& beat,
    const double mood,
    const RgbPalette& palette) const {
    const std::size_t step = static_cast<std::size_t>(std::max(0.0, std::floor(beat.beat / scene.rate_beats)));
    const Rgb a = palette_color(palette, step);
    const Rgb b = palette_color(palette, step + 1U);
    const Rgb c = palette_color(palette, step + 2U);
    RgbParSceneOutput output{{a, a, a}, lerp(scene.dimmer_min, scene.dimmer_max, mood)};

    if (scene.type == "gradient") {
        output.colors = {a, b, c};
    } else if (scene.type == "sides") {
        output.colors = {a, b, a};
    } else if (scene.type == "breathe") {
        const double wave = 0.5 + std::sin(beat.beat / scene.rate_beats * 6.2831853072) * 0.5;
        output.master_scale = lerp(scene.dimmer_min, scene.dimmer_max, wave);
    } else if (scene.type == "slow_wave") {
        const double phase = beat.beat / scene.rate_beats * 6.2831853072;
        output.colors = {
            scaled(a, 0.45 + 0.55 * (0.5 + std::sin(phase) * 0.5)),
            scaled(b, 0.45 + 0.55 * (0.5 + std::sin(phase - 2.0943951024) * 0.5)),
            scaled(c, 0.45 + 0.55 * (0.5 + std::sin(phase - 4.1887902048) * 0.5)),
        };
    } else if (scene.type == "alternate") {
        output.colors = step % 2U == 0U ? std::array<Rgb, 3>{a, b, c} : std::array<Rgb, 3>{c, b, a};
    } else if (scene.type == "chase") {
        const std::size_t active = step % output.colors.size();
        output.colors = {scaled(b, 0.18), scaled(b, 0.18), scaled(b, 0.18)};
        output.colors.at(active) = a;
    } else if (scene.type == "beat_pulse") {
        const double hit = std::exp(-beat.phase * 5.0);
        output.master_scale = lerp(scene.dimmer_min, scene.dimmer_max, hit);
        output.colors = {a, b, a};
    }

    output.master_scale = clamp01(output.master_scale);
    return output;
}

std::string RgbParSceneLibrary::labels_json() const {
    std::ostringstream out;
    out << R"({"auto":"Automatisch nach Fluter-Mood")";
    for (const RgbParSceneDefinition& scene : scenes_) {
        out << ',' << '"' << scene.id << R"(":")" << scene.label << '"';
    }
    out << '}';
    return out.str();
}

}  // namespace lightengine
