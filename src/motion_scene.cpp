#include "lightengine/motion_scene.hpp"

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

bool bool_field(const std::string& object, const std::string& key, const bool fallback) {
    const std::regex pattern{'"' + key + R"json("\s*:\s*(true|false))json"};
    std::smatch match;
    return std::regex_search(object, match, pattern) ? match[1].str() == "true" : fallback;
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

std::vector<MotionPoint> points_field(const std::string& object) {
    const std::size_t key = object.find("\"points\"");
    const std::size_t open = key == std::string::npos ? std::string::npos : object.find('[', key);
    if (open == std::string::npos) {
        return {};
    }
    std::size_t close = std::string::npos;
    int depth = 0;
    for (std::size_t index = open; index < object.size(); ++index) {
        if (object.at(index) == '[') {
            ++depth;
        } else if (object.at(index) == ']' && --depth == 0) {
            close = index;
            break;
        }
    }
    const std::string values = close == std::string::npos ? object.substr(open) : object.substr(open, close - open + 1U);
    const std::regex pair_pattern{R"json(\[\s*(-?[0-9]+(?:\.[0-9]+)?)\s*,\s*(-?[0-9]+(?:\.[0-9]+)?)\s*\])json"};
    std::vector<MotionPoint> points;
    for (auto item = std::sregex_iterator{values.begin(), values.end(), pair_pattern}; item != std::sregex_iterator{}; ++item) {
        points.push_back(MotionPoint{clamp01(std::stod((*item)[1].str())), clamp01(std::stod((*item)[2].str()))});
    }
    return points;
}

std::uint64_t mix_seed(std::uint64_t value) {
    value ^= value >> 30U;
    value *= 0xbf58476d1ce4e5b9ULL;
    value ^= value >> 27U;
    value *= 0x94d049bb133111ebULL;
    return value ^ (value >> 31U);
}

double seeded_unit(const std::uint64_t seed) {
    return static_cast<double>(mix_seed(seed) & 0xFFFFFFU) / static_cast<double>(0xFFFFFFU);
}

MotionPoint point_at(const MotionSceneDefinition& scene, const std::size_t index) {
    return scene.points.empty() ? MotionPoint{} : scene.points.at(index % scene.points.size());
}

}  // namespace

void MotionSceneLibrary::load_from_file(const std::string& path) {
    std::ifstream file{path};
    if (!file) {
        throw std::runtime_error{"cannot open moving-head scenes: " + path};
    }
    const std::string text{std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{}};
    std::vector<MotionSceneDefinition> loaded;
    for (const std::string& object : scene_objects(text)) {
        MotionSceneDefinition scene;
        scene.id = string_field(object, "id");
        scene.label = string_field(object, "name", scene.id);
        scene.type = string_field(object, "type");
        scene.grouping = string_field(object, "grouping", "fixture");
        scene.points = points_field(object);
        scene.speed_low = number_field(object, "speed_low", 0.25);
        scene.speed_high = number_field(object, "speed_high", 0.75);
        scene.step_low = number_field(object, "step_low", 8.0);
        scene.step_high = number_field(object, "step_high", 3.0);
        scene.dimmer_min = number_field(object, "dimmer_min", 0.35);
        scene.dimmer_max = number_field(object, "dimmer_max", 1.0);
        scene.decay = number_field(object, "decay", 5.0);
        scene.x_amount = number_field(object, "x_amount", 0.3);
        scene.y_amount = number_field(object, "y_amount", 0.2);
        scene.beat_multiplier = std::max(0.25, number_field(object, "beat_multiplier", 1.0));
        scene.energy_min = clamp01(number_field(object, "energy_min", 0.0));
        scene.energy_max = clamp01(number_field(object, "energy_max", 1.0));
        scene.allow_shake = bool_field(object, "allow_shake", false);
        if (!scene.id.empty() && !scene.type.empty()) {
            loaded.push_back(std::move(scene));
        }
    }
    if (loaded.empty()) {
        throw std::runtime_error{"moving-head scene file contains no scenes"};
    }
    scenes_ = std::move(loaded);
}

const std::vector<MotionSceneDefinition>& MotionSceneLibrary::scenes() const noexcept {
    return scenes_;
}

const MotionSceneDefinition* MotionSceneLibrary::find(const std::string_view id) const noexcept {
    const auto item = std::find_if(scenes_.begin(), scenes_.end(), [&](const MotionSceneDefinition& scene) { return scene.id == id; });
    return item == scenes_.end() ? nullptr : &*item;
}

MotionTarget MotionSceneLibrary::evaluate(
    const MotionSceneDefinition& scene,
    const std::size_t fixture_index,
    const std::size_t fixture_count,
    const BeatSnapshot& beat,
    const double mood,
    const std::uint64_t seed,
    const bool beat_pulse_enabled) const {
    const double speed = lerp(scene.speed_low, scene.speed_high, mood);
    const double step_beats = std::max(1.0, lerp(scene.step_low, scene.step_high, mood));
    const std::size_t step = static_cast<std::size_t>(std::max(0.0, std::floor(beat.beat / step_beats)));
    const std::size_t pair = fixture_index / 2U;
    const double hit = beat_pulse_enabled ? std::exp(-beat.phase * scene.decay) : 0.0;
    MotionTarget target;

    if (scene.type == "fixed") {
        const MotionPoint point = point_at(scene, 0);
        target = {point.x, point.y, lerp(scene.dimmer_min, scene.dimmer_max, hit)};
    } else if (scene.type == "point_chase") {
        const MotionPoint point = point_at(scene, step + fixture_index % 2U);
        target = {point.x, point.y, lerp(scene.dimmer_min, scene.dimmer_max, hit)};
    } else if (scene.type == "sine_pan") {
        const double phase = beat.beat * speed + static_cast<double>(fixture_index % 2U) * 3.1415926536;
        target.x = 0.5 + std::sin(phase) * scene.x_amount;
        target.y = point_at(scene, 0).y + std::sin(beat.beat * speed * 0.31 + static_cast<double>(fixture_index)) * scene.y_amount;
        target.dimmer_scale = lerp(scene.dimmer_min, scene.dimmer_max, hit);
    } else if (scene.type == "sine_depth") {
        const double phase = beat.beat * speed + static_cast<double>(pair) * 3.1415926536;
        target.x = point_at(scene, fixture_index).x;
        target.y = 0.5 + std::sin(phase) * scene.y_amount;
        target.dimmer_scale = lerp(scene.dimmer_min, scene.dimmer_max, hit);
    } else if (scene.type == "pair_swap") {
        const bool swap = step % 2U != 0U;
        const MotionPoint point = point_at(scene, pair + (swap ? 1U : 0U));
        target = {point.x, point.y, lerp(scene.dimmer_min, scene.dimmer_max, hit)};
    } else if (scene.type == "alternating_pairs") {
        const MotionPoint point = point_at(scene, pair);
        const bool on = (static_cast<std::size_t>(std::floor(beat.beat * scene.beat_multiplier)) + pair) % 2U == 0U;
        target = {point.x, point.y, on ? scene.dimmer_max : scene.dimmer_min};
    } else if (scene.type == "indexed_points") {
        const std::size_t offset = step % 2U == 0U ? fixture_index : fixture_count - 1U - fixture_index;
        const MotionPoint point = point_at(scene, offset);
        target = {point.x, point.y, lerp(scene.dimmer_min, scene.dimmer_max, hit)};
    } else if (scene.type == "side_pingpong") {
        const MotionPoint point = point_at(scene, step + fixture_index);
        target = {point.x, point.y, beat.phase < 0.55 ? scene.dimmer_max : scene.dimmer_min};
    } else if (scene.type == "fan") {
        const MotionPoint point = point_at(scene, fixture_index);
        target.x = point.x;
        target.y = point.y + std::sin(beat.beat * speed + static_cast<double>(fixture_index)) * scene.y_amount;
        target.dimmer_scale = lerp(scene.dimmer_min, scene.dimmer_max, hit);
    } else if (scene.type == "orbit") {
        const double angle = beat.beat * speed + static_cast<double>(pair) * 3.1415926536 +
            static_cast<double>(fixture_index % 2U) * 0.65;
        target.x = 0.5 + std::sin(angle) * scene.x_amount;
        target.y = 0.5 + std::cos(angle) * scene.y_amount;
        target.dimmer_scale = lerp(scene.dimmer_min, scene.dimmer_max, hit);
    } else {
        const std::size_t group = scene.grouping == "pair" ? pair : fixture_index;
        const std::uint64_t random_seed = seed + static_cast<std::uint64_t>(step) * 97U + static_cast<std::uint64_t>(group) * 211U;
        if (scene.points.empty()) {
            target.x = 0.16 + seeded_unit(random_seed) * 0.68;
            target.y = 0.28 + seeded_unit(random_seed + 1U) * 0.58;
        } else {
            const MotionPoint point = point_at(scene, static_cast<std::size_t>(mix_seed(random_seed)));
            target.x = point.x;
            target.y = point.y;
        }
        target.dimmer_scale = beat.phase < lerp(0.18, 0.38, mood) ? scene.dimmer_max : scene.dimmer_min;
    }

    target.x = clamp01(target.x);
    target.y = clamp01(target.y);
    target.dimmer_scale = clamp01(target.dimmer_scale);
    return target;
}

std::string MotionSceneLibrary::labels_json() const {
    std::ostringstream out;
    out << '{';
    bool first = true;
    for (const MotionSceneDefinition& scene : scenes_) {
        if (scene.id.starts_with("standby_") || scene.id.starts_with("mh_")) {
            continue;
        }
        if (!first) {
            out << ',';
        }
        first = false;
        out << '"' << scene.id << R"(":")" << scene.label << '"';
    }
    out << '}';
    return out.str();
}

}  // namespace lightengine
