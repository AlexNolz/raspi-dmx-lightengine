#include "lightengine/show_layers.hpp"

#include <algorithm>
#include <utility>

namespace lightengine {

namespace {

std::uint64_t mix_seed(std::uint64_t value) {
    value ^= value >> 30U;
    value *= 0xbf58476d1ce4e5b9ULL;
    value ^= value >> 27U;
    value *= 0x94d049bb133111ebULL;
    return value ^ (value >> 31U);
}

std::uint64_t text_seed(const std::string_view text) {
    std::uint64_t seed = 1469598103934665603ULL;
    for (const unsigned char character : text) {
        seed ^= character;
        seed *= 1099511628211ULL;
    }
    return seed;
}

std::int64_t color_hold_beats(const MusicalSection section) {
    switch (section) {
        case MusicalSection::calm: return 64;
        case MusicalSection::groove: return 32;
        case MusicalSection::buildup: return 16;
        case MusicalSection::peak: return 8;
        case MusicalSection::release: return 32;
    }
    return 32;
}

}  // namespace

ColorLayerSelection ColorLayer::resolve(
    const RgbSceneMixer& mixer,
    const ShowLayerContext& context,
    const std::vector<std::string>& allowed_palette_ids) const {
    ColorLayerSelection result;
    result.hold_beats = color_hold_beats(context.dynamics.section);
    result.epoch = std::max<std::int64_t>(0, context.beat.position) / result.hold_beats;
    const std::uint64_t base_seed = mix_seed(context.show_seed ^ text_seed(context.preset));
    const std::uint64_t selection_step = static_cast<std::uint64_t>(result.epoch) + context.color_nonce;
    if (allowed_palette_ids.empty()) {
        result.palette = mixer.palette_for_preset(
            context.preset, static_cast<std::int64_t>((base_seed + selection_step) & 0x7fffffffffffffffULL));
    } else {
        const std::size_t index = static_cast<std::size_t>((base_seed + selection_step) % allowed_palette_ids.size());
        result.palette = mixer.palette_by_id(allowed_palette_ids.at(index));
    }
    const std::size_t count = std::min(result.palette.colors.size(), result.palette.color_names.size());
    if (count > 1U) {
        const std::size_t rotation = static_cast<std::size_t>((base_seed + selection_step) % count);
        std::rotate(result.palette.colors.begin(), result.palette.colors.begin() + static_cast<std::ptrdiff_t>(rotation), result.palette.colors.end());
        std::rotate(result.palette.color_names.begin(), result.palette.color_names.begin() + static_cast<std::ptrdiff_t>(rotation), result.palette.color_names.end());
    }
    return result;
}

std::string SceneLayerPlanner::select_rgb_scene(
    const std::vector<std::string>& enabled,
    const std::vector<RgbSceneDefinition>& definitions,
    const ShowLayerContext& context,
    const std::uint64_t selection_nonce) const {
    std::vector<std::string> candidates;
    for (const std::string& id : enabled) {
        const auto definition = std::find_if(definitions.begin(), definitions.end(), [&](const RgbSceneDefinition& item) {
            return item.id == id;
        });
        const bool strong_effect_allowed = definition == definitions.end() || definition->energy_min < 0.85 ||
            context.dynamics.highpoint();
        if (strong_effect_allowed && (definition == definitions.end() ||
            (context.dynamics.energy >= definition->energy_min && context.dynamics.energy <= definition->energy_max))) {
            candidates.push_back(id);
        }
    }
    if (candidates.empty()) {
        return "breathe";
    }
    const std::int64_t phrase = std::max<std::int64_t>(0, context.beat.position) / 16;
    const std::uint64_t seed = mix_seed(
        context.show_seed ^ text_seed(context.preset) ^ static_cast<std::uint64_t>(phrase) * 131U ^ selection_nonce * 977U);
    return candidates.at(static_cast<std::size_t>(seed % candidates.size()));
}

std::string SceneLayerPlanner::select_motion_scene(
    const std::vector<std::string>& enabled,
    const MotionSceneLibrary& library,
    const ShowLayerContext& context) const {
    std::vector<std::string> candidates;
    for (const std::string& id : enabled) {
        const MotionSceneDefinition* definition = library.find(id);
        const bool strong_effect_allowed = definition == nullptr || definition->energy_min < 0.85 ||
            context.dynamics.highpoint();
        if (strong_effect_allowed && (definition == nullptr ||
            (context.dynamics.energy >= definition->energy_min && context.dynamics.energy <= definition->energy_max))) {
            candidates.push_back(id);
        }
    }
    if (candidates.empty()) {
        candidates.push_back("center_pulse");
    }
    const std::int64_t phrase = std::max<std::int64_t>(0, context.beat.position) / 16;
    const std::uint64_t seed = mix_seed(
        context.show_seed ^ text_seed(context.preset) ^ static_cast<std::uint64_t>(phrase) * 313U ^ 0x4d4f54494f4eULL);
    return candidates.at(static_cast<std::size_t>(seed % candidates.size()));
}

GoboLayerSelection GoboLayer::resolve(
    const Zkymzl11Profile& profile,
    const ShowLayerContext& context,
    const GoboLayerRequest& request,
    const std::size_t fixture_index,
    const std::vector<std::string>& allowed_gobos) const {
    GoboLayerSelection result;
    result.value = profile.gobo_value("open").value_or(18);
    if (!request.enabled || profile.gobos.empty()) {
        return result;
    }
    if (request.mode == "static") {
        result.value = profile.gobo_value(std::string{request.selected_gobo}).value_or(result.value);
        return result;
    }

    std::vector<const FixtureWheelSlot*> candidates;
    for (const FixtureWheelSlot& gobo : profile.gobos) {
        if (allowed_gobos.empty() || std::find(allowed_gobos.begin(), allowed_gobos.end(), gobo.id) != allowed_gobos.end()) {
            candidates.push_back(&gobo);
        }
    }
    if (candidates.empty()) {
        return result;
    }

    if (request.mode == "musical") {
        result.hold_beats = context.dynamics.highpoint() && request.fast_peak_enabled ? 1 :
            context.dynamics.section == MusicalSection::buildup ? 8 : 16;
    } else if (request.mode == "phrase_random") {
        result.hold_beats = 16;
    } else if (request.mode == "random_beat") {
        result.hold_beats = 1;
    } else {
        result.hold_beats = 2;
    }

    const std::int64_t epoch = std::max<std::int64_t>(0, context.beat.position) / result.hold_beats;
    const std::size_t pair = fixture_index / 2U;
    const std::uint64_t seed = mix_seed(
        context.show_seed ^ static_cast<std::uint64_t>(pair) * 37U ^ 0x474f424fULL);
    const std::size_t base = static_cast<std::size_t>(seed % candidates.size());
    const std::size_t step = static_cast<std::size_t>(epoch) * 5U + pair * 3U;
    result.value = candidates.at((base + step) % candidates.size())->value;

    if (request.highpoint_only && !context.dynamics.highpoint()) {
        result.value = profile.gobo_value("open").value_or(result.value);
    }
    const std::uint8_t open = profile.gobo_value("open").value_or(18);
    if (request.shake_enabled && context.dynamics.highpoint() && result.value == open && candidates.size() > 1U) {
        for (std::size_t offset = 1U; offset < candidates.size(); ++offset) {
            const std::uint8_t candidate = candidates.at((base + step + offset) % candidates.size())->value;
            if (candidate != open) {
                result.value = candidate;
                break;
            }
        }
    }
    result.shaking = request.shake_enabled && request.scene_allows_shake && context.dynamics.highpoint() &&
        context.mood >= request.shake_mood_threshold && result.value != open;
    if (result.shaking) {
        result.value = static_cast<std::uint8_t>(std::clamp(static_cast<int>(result.value) + 64, 64, 127));
    }
    return result;
}

}  // namespace lightengine
