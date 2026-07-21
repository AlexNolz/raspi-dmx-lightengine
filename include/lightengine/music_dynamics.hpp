#pragma once

#include "lightengine/beat_clock.hpp"
#include "lightengine/os2l_event.hpp"

#include <string>
#include <string_view>

namespace lightengine {

enum class MusicalSection {
    calm,
    groove,
    buildup,
    peak,
    release,
};

struct MusicDynamicsSnapshot final {
    MusicalSection section{MusicalSection::groove};
    double energy{0.5};
    double tempo_energy{0.5};
    double strength_trend{};
    bool strength_available{false};
    bool phrase_boundary{false};

    [[nodiscard]] bool highpoint() const noexcept {
        return section == MusicalSection::peak;
    }
};

class MusicDynamicsEstimator final {
public:
    void reset() noexcept;
    void on_beat(const Os2lBeatEvent& beat) noexcept;
    [[nodiscard]] MusicDynamicsSnapshot snapshot(
        const BeatSnapshot& beat,
        double mood,
        std::string_view preset) const noexcept;

private:
    double fast_strength_{0.5};
    double slow_strength_{0.5};
    bool strength_initialized_{false};
};

[[nodiscard]] const char* musical_section_name(MusicalSection section) noexcept;

}  // namespace lightengine
