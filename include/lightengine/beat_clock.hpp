#pragma once

#include "lightengine/os2l_event.hpp"

#include <chrono>
#include <cstdint>

namespace lightengine {

struct BeatSnapshot final {
    double beat{0.0};
    double phase{0.0};
    std::int64_t position{};
    double bpm{120.0};
    double strength{0.5};
    bool locked_to_os2l{false};
    bool strength_available{false};
};

class BeatClock final {
public:
    void on_beat(const Os2lBeatEvent& beat, std::chrono::steady_clock::time_point received_at);
    [[nodiscard]] BeatSnapshot snapshot(std::chrono::steady_clock::time_point now) const;

private:
    std::int64_t position_{};
    double bpm_{120.0};
    double strength_{0.5};
    bool strength_available_{false};
    std::chrono::steady_clock::time_point last_beat_at_{};
    bool locked_{false};
};

}  // namespace lightengine
