#include "lightengine/music_dynamics.hpp"

#include <algorithm>
#include <cmath>

namespace lightengine {

namespace {

double clamp01(const double value) {
    return std::clamp(value, 0.0, 1.0);
}

double preset_bias(const std::string_view preset) {
    if (preset == "lounge") {
        return 0.20;
    }
    if (preset == "rave") {
        return 0.90;
    }
    if (preset == "rgb_hard") {
        return 0.78;
    }
    if (preset == "game_show") {
        return 0.64;
    }
    return 0.56;
}

MusicalSection scripted_section(const std::int64_t position) {
    const std::int64_t phrase = std::max<std::int64_t>(0, position) / 16;
    switch (phrase % 8) {
        case 0: return MusicalSection::calm;
        case 1: return MusicalSection::groove;
        case 2: return MusicalSection::buildup;
        case 3: return MusicalSection::peak;
        case 4: return MusicalSection::release;
        case 5: return MusicalSection::groove;
        case 6: return MusicalSection::buildup;
        default: return MusicalSection::peak;
    }
}

double section_energy(const MusicalSection section) {
    switch (section) {
        case MusicalSection::calm: return 0.22;
        case MusicalSection::groove: return 0.48;
        case MusicalSection::buildup: return 0.68;
        case MusicalSection::peak: return 0.96;
        case MusicalSection::release: return 0.35;
    }
    return 0.5;
}

}  // namespace

void MusicDynamicsEstimator::reset() noexcept {
    fast_strength_ = 0.5;
    slow_strength_ = 0.5;
    strength_initialized_ = false;
}

void MusicDynamicsEstimator::on_beat(const Os2lBeatEvent& beat) noexcept {
    if (!beat.strength_available) {
        return;
    }
    const double strength = clamp01(beat.strength);
    if (!strength_initialized_) {
        fast_strength_ = strength;
        slow_strength_ = strength;
        strength_initialized_ = true;
        return;
    }
    fast_strength_ += (strength - fast_strength_) * 0.38;
    slow_strength_ += (strength - slow_strength_) * 0.055;
}

MusicDynamicsSnapshot MusicDynamicsEstimator::snapshot(
    const BeatSnapshot& beat,
    const double mood,
    const std::string_view preset) const noexcept {
    MusicDynamicsSnapshot result;
    result.tempo_energy = clamp01((beat.bpm - 80.0) / 100.0);
    result.strength_available = beat.strength_available && strength_initialized_;
    result.strength_trend = result.strength_available ? std::clamp(fast_strength_ - slow_strength_, -1.0, 1.0) : 0.0;
    result.phrase_boundary = beat.position >= 0 && beat.position % 16 < 4;
    result.section = scripted_section(beat.position);

    const double clamped_mood = clamp01(mood);
    if (result.section == MusicalSection::peak && clamped_mood < 0.42) {
        result.section = MusicalSection::groove;
    } else if (result.section == MusicalSection::buildup && clamped_mood < 0.25) {
        result.section = MusicalSection::calm;
    }

    if (result.strength_available) {
        if (result.phrase_boundary && result.strength_trend > 0.10 && clamped_mood >= 0.35) {
            result.section = MusicalSection::peak;
        } else if (result.strength_trend < -0.13) {
            result.section = MusicalSection::release;
        }
    }

    const double scripted = section_energy(result.section);
    const double base = clamped_mood * 0.48 + preset_bias(preset) * 0.34 + result.tempo_energy * 0.18;
    result.energy = clamp01(scripted * 0.58 + base * 0.42 + std::max(0.0, result.strength_trend) * 0.35);
    return result;
}

const char* musical_section_name(const MusicalSection section) noexcept {
    switch (section) {
        case MusicalSection::calm: return "calm";
        case MusicalSection::groove: return "groove";
        case MusicalSection::buildup: return "buildup";
        case MusicalSection::peak: return "peak";
        case MusicalSection::release: return "release";
    }
    return "groove";
}

}  // namespace lightengine
