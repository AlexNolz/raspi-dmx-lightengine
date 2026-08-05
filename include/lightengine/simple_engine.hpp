#pragma once

#include "lightengine/artnet_sender.hpp"
#include "lightengine/beat_clock.hpp"
#include "lightengine/color.hpp"
#include "lightengine/control_command.hpp"
#include "lightengine/dmx.hpp"
#include "lightengine/fixture_runtime.hpp"
#include "lightengine/os2l_event.hpp"
#include "lightengine/motion_scene.hpp"
#include "lightengine/music_dynamics.hpp"
#include "lightengine/project.hpp"
#include "lightengine/rgb_par_scene.hpp"
#include "lightengine/rgb_scene.hpp"
#include "lightengine/show_layers.hpp"

#include <chrono>
#include <array>
#include <cstdint>
#include <mutex>
#include <string>
#include <string_view>
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
    [[nodiscard]] bool output_active(std::chrono::steady_clock::time_point now) const;

    void mark_artnet_packet_sent();

private:
    void apply_preset_locked(const std::string& preset);
    void select_next_effect_locked(const MusicDynamicsSnapshot* dynamics = nullptr);
    [[nodiscard]] std::string active_effect_label_locked() const;
    void render_safe_moving_head_blackout(DmxFrame& frame, bool reset_active) const;
    void render_moving_heads(
        DmxFrame& frame,
        const BeatSnapshot& beat,
        std::chrono::steady_clock::time_point now,
        const RgbPalette& palette,
        std::string_view scene_override = {});
    void render_auxiliary_fixtures(DmxFrame& frame, const BeatSnapshot& beat, std::chrono::steady_clock::time_point now) const;
    void render_rgb_pars(DmxFrame& frame, const BeatSnapshot& beat, const RgbPalette& palette) const;

    mutable std::mutex mutex_;
    SimpleEngineConfig config_;
    BeatClock beat_clock_;
    MusicDynamicsEstimator music_dynamics_;
    bool running_{false};
    bool blackout_{false};
    bool blackout_held_{false};
    bool whiteout_held_{false};
    bool color_strobe_held_{false};
    bool strobe_out_held_{false};
    bool led_layer_enabled_{true};
    bool motion_layer_enabled_{true};
    bool fog_layer_enabled_{false};
    bool strobe_armed_{false};
    bool fog_armed_{false};
    bool strobe_beat_pulse_{false};
    bool motion_beat_pulse_enabled_{true};
    bool gobo_enabled_{false};
    bool gobo_highpoint_only_{false};
    bool gobo_shake_enabled_{false};
    bool gobo_fast_peak_enabled_{true};
    bool manual_color_enabled_{false};
    bool manual_color_use_raw_{false};
    bool rgb_par_zone_linked_{false};
    double master_{1.0};
    double led_master_{1.0};
    double motion_master_{1.0};
    double strobe_master_{1.0};
    double strobe_speed_{1.0};
    double gobo_shake_mood_threshold_{0.62};
    std::uint8_t mood_{58};
    std::string preset_{"club"};
    std::string motion_mode_{"auto"};
    std::string gobo_mode_{"beat_step"};
    std::string selected_gobo_{"open"};
    std::string selected_color_{"white"};
    std::uint8_t manual_color_value_{8};
    std::uint8_t rgb_par_zone_mood_{35};
    std::string rgb_par_zone_scene_{"auto"};
    std::vector<std::string> active_effects_{"breathe", "pulse", "comet"};
    std::string selected_effect_{"breathe"};
    std::vector<std::string> active_scenes_{"center_pulse"};
    std::vector<std::string> active_palettes_;
    std::vector<std::string> active_gobos_;
    std::int64_t last_effect_change_position_{-1};
    std::array<std::uint16_t, 4> moving_head_starts_{51, 62, 73, 84};
    std::array<std::uint16_t, 3> rgb_par_starts_{100, 110, 120};
    std::uint16_t strobe_start_{1};
    std::uint16_t fog_start_{95};
    std::chrono::steady_clock::time_point whiteout_until_{};
    std::chrono::steady_clock::time_point color_strobe_until_{};
    std::chrono::steady_clock::time_point strobe_out_until_{};
    std::chrono::steady_clock::time_point fog_until_{};
    std::chrono::steady_clock::time_point reset_until_{};
    std::chrono::steady_clock::time_point last_music_beat_at_{};
    std::uint64_t os2l_messages_{};
    std::uint64_t artnet_packets_{};
    std::chrono::steady_clock::time_point last_os2l_at_{};
    std::vector<Rgb> preview_;
    RgbWashBar bar1_;
    RgbWashBar bar2_;
    RgbSceneMixer rgb_scenes_;
    RgbParSceneLibrary rgb_par_scenes_;
    MotionSceneLibrary motion_scenes_;
    ColorLayer color_layer_;
    SceneLayerPlanner scene_layer_planner_;
    GoboLayer gobo_layer_;
    Zkymzl11Profile moving_head_profile_;
    ShowProject project_;
    std::array<Zkymzl11Look, 4> last_moving_head_looks_{};
    std::uint64_t show_seed_{0x6c69676874656e67ULL};
    std::uint64_t manual_selection_nonce_{};
    std::uint64_t color_selection_nonce_{};
};

}  // namespace lightengine
