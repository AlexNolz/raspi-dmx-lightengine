#pragma once

#include "lightengine/artnet_sender.hpp"
#include "lightengine/beat_clock.hpp"
#include "lightengine/color.hpp"
#include "lightengine/control_command.hpp"
#include "lightengine/dmx.hpp"
#include "lightengine/fixture_runtime.hpp"
#include "lightengine/os2l_event.hpp"
#include "lightengine/rgb_scene.hpp"

#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace lightengine {

struct SimpleEngineConfig final {
    std::string artnet_host{"127.0.0.1"};
    std::uint16_t artnet_universe{};
    std::uint16_t bar1_start{3};
    std::uint16_t bar2_start{27};
    std::uint8_t segments_per_bar{8};
};

class SimpleEngine final {
public:
    explicit SimpleEngine(SimpleEngineConfig config);

    void apply_os2l_event(const Os2lEvent& event, std::chrono::steady_clock::time_point received_at);
    void apply_control_command(const ControlCommand& command);

    [[nodiscard]] DmxFrame render_frame(std::chrono::steady_clock::time_point now);
    [[nodiscard]] std::string state_json(std::chrono::steady_clock::time_point now) const;
    [[nodiscard]] ArtNetEndpoint artnet_endpoint() const;

    void mark_artnet_packet_sent();

private:
    void apply_preset_locked(const std::string& preset);
    void render_safe_moving_head_blackout(DmxFrame& frame) const;

    mutable std::mutex mutex_;
    SimpleEngineConfig config_;
    BeatClock beat_clock_;
    bool running_{false};
    bool blackout_{false};
    bool blackout_held_{false};
    bool whiteout_held_{false};
    bool color_strobe_held_{false};
    bool strobe_out_held_{false};
    bool led_layer_enabled_{true};
    bool motion_layer_enabled_{false};
    bool strobe_armed_{false};
    bool strobe_beat_pulse_{false};
    bool gobo_enabled_{false};
    bool gobo_highpoint_only_{false};
    bool gobo_shake_enabled_{false};
    double master_{1.0};
    double led_master_{1.0};
    double motion_master_{1.0};
    double strobe_master_{1.0};
    double strobe_speed_{1.0};
    double gobo_shake_mood_threshold_{0.62};
    std::uint8_t mood_{58};
    std::string preset_{"club"};
    std::string gobo_mode_{"beat_step"};
    std::vector<std::string> active_effects_{"rgb_static", "rgb_beat_pulse", "rgb_comet"};
    std::vector<std::string> active_scenes_{"mh_center_pulse"};
    std::uint64_t os2l_messages_{};
    std::uint64_t artnet_packets_{};
    std::chrono::steady_clock::time_point last_os2l_at_{};
    std::vector<Rgb> preview_;
    RgbWashBar bar1_;
    RgbWashBar bar2_;
    RgbSceneMixer rgb_scenes_;
};

}  // namespace lightengine
