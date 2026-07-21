#include "lightengine/beat_clock.hpp"

#include <algorithm>
#include <cmath>

namespace lightengine {

namespace {

double clamp01(const double value) {
    return std::max(0.0, std::min(1.0, value));
}

}  // namespace

void BeatClock::on_beat(const Os2lBeatEvent& beat, const std::chrono::steady_clock::time_point received_at) {
    position_ = beat.position;
    bpm_ = std::max(20.0, std::min(260.0, beat.bpm));
    strength_ = clamp01(beat.strength);
    last_beat_at_ = received_at;
    locked_ = true;
}

BeatSnapshot BeatClock::snapshot(const std::chrono::steady_clock::time_point now) const {
    const double beat_seconds = 60.0 / std::max(1.0, bpm_);
    double phase = 0.0;
    bool locked = false;
    if (locked_) {
        const std::chrono::duration<double> age = now - last_beat_at_;
        phase = std::max(0.0, age.count()) / beat_seconds;
        locked = age.count() < 3.0;
    }
    const double wrapped_phase = phase - std::floor(phase);
    return BeatSnapshot{
        static_cast<double>(position_) + phase,
        wrapped_phase,
        position_,
        bpm_,
        strength_,
        locked,
    };
}

}  // namespace lightengine
