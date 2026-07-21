#include "lightengine/simple_engine.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <variant>

namespace lightengine {

namespace {

double clamp01(const double value) {
    return std::max(0.0, std::min(1.0, value));
}

std::uint8_t to_dmx(const double value) {
    return static_cast<std::uint8_t>(std::lround(clamp01(value) * 255.0));
}

std::string json_bool(const bool value) {
    return value ? "true" : "false";
}

void json_string_array(std::ostringstream& out, const std::vector<std::string>& values) {
    out << '[';
    for (std::size_t index = 0; index < values.size(); ++index) {
        if (index != 0) {
            out << ',';
        }
        out << '"' << values.at(index) << '"';
    }
    out << ']';
}

void set_enabled(std::vector<std::string>& values, const std::string& value, const bool enabled) {
    const auto item = std::find(values.begin(), values.end(), value);
    if (enabled && item == values.end()) {
        values.push_back(value);
        std::sort(values.begin(), values.end());
    } else if (!enabled && item != values.end()) {
        values.erase(item);
    }
}

}  // namespace

SimpleEngine::SimpleEngine(SimpleEngineConfig config)
    : config_{std::move(config)},
      preview_(static_cast<std::size_t>(config_.segments_per_bar) * 2U),
      bar1_{DmxAddress{config_.bar1_start}, config_.segments_per_bar},
      bar2_{DmxAddress{config_.bar2_start}, config_.segments_per_bar} {
    rgb_scenes_.load_palettes_from_file("shows/color_palettes.json");
    rgb_scenes_.load_scene_definitions_from_file("shows/rgb_scenes.json");
}

void SimpleEngine::apply_os2l_event(const Os2lEvent& event, const std::chrono::steady_clock::time_point received_at) {
    std::lock_guard lock{mutex_};
    ++os2l_messages_;
    last_os2l_at_ = received_at;
    std::visit(
        [&](const auto& typed_event) {
            using Event = std::decay_t<decltype(typed_event)>;
            if constexpr (std::is_same_v<Event, Os2lBeatEvent>) {
                beat_clock_.on_beat(typed_event, received_at);
            } else if constexpr (std::is_same_v<Event, Os2lButtonEvent>) {
                if (typed_event.name == "blackout") {
                    blackout_held_ = typed_event.pressed;
                } else if (typed_event.name == "whiteout") {
                    whiteout_held_ = typed_event.pressed;
                } else if (typed_event.name == "colorstrobe") {
                    color_strobe_held_ = typed_event.pressed;
                } else if (typed_event.name == "strobeout") {
                    strobe_out_held_ = typed_event.pressed;
                }
            }
        },
        event);
}

void SimpleEngine::apply_control_command(const ControlCommand& command) {
    std::lock_guard lock{mutex_};
    std::visit(
        [&](const auto& typed_command) {
            using Command = std::decay_t<decltype(typed_command)>;
            if constexpr (std::is_same_v<Command, SetRunningCommand>) {
                running_ = typed_command.running;
            } else if constexpr (std::is_same_v<Command, SetBlackoutCommand>) {
                blackout_ = typed_command.blackout;
            } else if constexpr (std::is_same_v<Command, SetMoodCommand>) {
                mood_ = typed_command.mood;
                if (typed_command.mark_custom) {
                    preset_ = "custom";
                }
            } else if constexpr (std::is_same_v<Command, SetMasterCommand>) {
                master_ = typed_command.value;
            } else if constexpr (std::is_same_v<Command, SetOutputMasterCommand>) {
                if (typed_command.target == OutputMasterTarget::led) {
                    led_master_ = typed_command.value;
                } else {
                    motion_master_ = typed_command.value;
                }
            } else if constexpr (std::is_same_v<Command, SetLayerCommand>) {
                if (typed_command.layer == LayerId::led_bars) {
                    led_layer_enabled_ = typed_command.enabled;
                } else if (typed_command.layer == LayerId::motion) {
                    motion_layer_enabled_ = typed_command.enabled;
                }
            } else if constexpr (std::is_same_v<Command, SetFixtureArmedCommand>) {
                if (typed_command.fixture == ArmedFixtureId::strobe) {
                    strobe_armed_ = typed_command.armed;
                }
            } else if constexpr (std::is_same_v<Command, SetStrobeBeatPulseCommand>) {
                strobe_beat_pulse_ = typed_command.enabled;
            } else if constexpr (std::is_same_v<Command, SetStrobeMasterCommand>) {
                strobe_master_ = typed_command.value;
            } else if constexpr (std::is_same_v<Command, SetStrobeSpeedCommand>) {
                strobe_speed_ = typed_command.value;
            } else if constexpr (std::is_same_v<Command, ApplyPresetCommand>) {
                apply_preset_locked(typed_command.preset);
            } else if constexpr (std::is_same_v<Command, ToggleEffectCommand>) {
                set_enabled(active_effects_, typed_command.effect, typed_command.enabled);
                preset_ = "custom";
            } else if constexpr (std::is_same_v<Command, ToggleMotionSceneCommand>) {
                set_enabled(active_scenes_, typed_command.scene, typed_command.enabled);
                preset_ = "custom";
            } else if constexpr (std::is_same_v<Command, SetHoldTriggerCommand>) {
                if (typed_command.trigger == LiveTriggerId::blackout) {
                    blackout_held_ = typed_command.held;
                } else if (typed_command.trigger == LiveTriggerId::whiteout) {
                    whiteout_held_ = typed_command.held;
                } else if (typed_command.trigger == LiveTriggerId::color_strobe) {
                    color_strobe_held_ = typed_command.held;
                } else if (typed_command.trigger == LiveTriggerId::strobe_out) {
                    strobe_out_held_ = typed_command.held;
                }
            }
        },
        command);
}

DmxFrame SimpleEngine::render_frame(const std::chrono::steady_clock::time_point now) {
    std::lock_guard lock{mutex_};
    DmxFrame frame{};
    std::fill(preview_.begin(), preview_.end(), Rgb{});
    bar1_.clear();
    bar2_.clear();

    if (!running_ || blackout_ || blackout_held_ || !led_layer_enabled_) {
        return frame;
    }

    const BeatSnapshot beat = beat_clock_.snapshot(now);
    const RgbSceneContext scene_context{
        beat,
        clamp01(master_ * led_master_),
        static_cast<double>(mood_) / 100.0,
    };
    const RgbPalette& palette = rgb_scenes_.palette_for_preset(preset_, beat.position / 16);
    rgb_scenes_.render(bar1_, active_effects_, scene_context, palette);
    rgb_scenes_.render(bar2_, active_effects_, scene_context, palette);
    if (whiteout_held_ || strobe_out_held_) {
        bar1_.set_all(Rgb{to_dmx(scene_context.master), to_dmx(scene_context.master), to_dmx(scene_context.master)});
        bar2_.set_all(Rgb{to_dmx(scene_context.master), to_dmx(scene_context.master), to_dmx(scene_context.master)});
    } else if (color_strobe_held_) {
        const auto beat_index = static_cast<std::size_t>(std::floor(beat.beat));
        for (std::size_t segment = 0; segment < bar1_.size(); ++segment) {
            const auto color_index = (segment + beat_index) % 3U;
            const Rgb color = color_index == 0U ? Rgb{to_dmx(scene_context.master), 0, 0}
                : color_index == 1U ? Rgb{0, to_dmx(scene_context.master), 0}
                                    : Rgb{0, 0, to_dmx(scene_context.master)};
            bar1_.set_wash(segment, color);
            bar2_.set_wash(segment, color);
        }
    }
    bar1_.render_to(frame);
    bar2_.render_to(frame);

    for (std::size_t index = 0; index < bar1_.size(); ++index) {
        preview_.at(index) = bar1_.wash_color(index);
    }
    for (std::size_t index = 0; index < bar2_.size(); ++index) {
        preview_.at(index + bar1_.size()) = bar2_.wash_color(index);
    }
    return frame;
}

std::string SimpleEngine::state_json(const std::chrono::steady_clock::time_point now) const {
    std::lock_guard lock{mutex_};
    const BeatSnapshot beat = beat_clock_.snapshot(now);
    const double os2l_age = last_os2l_at_ == std::chrono::steady_clock::time_point{}
        ? -1.0
        : std::chrono::duration<double>(now - last_os2l_at_).count();

    std::ostringstream out;
    out << R"({"running":)" << json_bool(running_)
        << R"(,"blackout":)" << json_bool(blackout_ || blackout_held_)
        << R"(,"active_effect":")" << (active_effects_.empty() ? std::string{"none"} : active_effects_.front())
        << R"(","active_effect_label":")" << (active_effects_.empty() ? std::string{"Keine RGB Szene"} : std::string{"RGB Szenen aktiv"})
        << R"(")"
        << R"(,"bpm":)" << beat.bpm
        << R"(,"beat_count":)" << static_cast<std::int64_t>(std::floor(beat.beat))
        << R"(,"beat_pos":)" << beat.position
        << R"(,"beat_phase":)" << beat.phase
        << R"(,"beat_strength":)" << beat.strength
        << R"(,"last_os2l_age":)" << (os2l_age < 0.0 ? std::string{"null"} : std::to_string(os2l_age))
        << R"(,"os2l_connected":)" << json_bool(beat.locked_to_os2l)
        << R"(,"os2l_connections":)" << os2l_messages_
        << R"(,"artnet_packets":)" << artnet_packets_
        << R"(,"last_error":"")";

    out << R"(,"config":{"artnet_host":")" << config_.artnet_host
        << R"(","artnet_universe":)" << config_.artnet_universe
        << R"(,"led_start_channel":)" << config_.bar1_start
        << R"(,"segment_count":)" << static_cast<int>(config_.segments_per_bar * 2U)
        << R"(,"master":)" << master_
        << R"(,"led_master":)" << led_master_
        << R"(,"motion_master":)" << motion_master_
        << R"(,"mood":)" << static_cast<int>(mood_)
        << R"(,"preset":")" << preset_
        << R"(","motion_mode":"auto")"
        << R"(,"enabled_effects":)";
    json_string_array(out, active_effects_);
    out << R"(,"enabled_motion_scenes":)";
    json_string_array(out, active_scenes_);
    out << R"(,"layers":{"led_bars":)" << json_bool(led_layer_enabled_)
        << R"(,"motion":)" << json_bool(motion_layer_enabled_)
        << R"(,"strobe":false,"fog":false})";
    out << R"(,"fixtures":{"led_bars":[)"
        << R"({"name":"LED Bar 1","start":)" << config_.bar1_start << R"(,"segments":8,"enabled":true},)"
        << R"({"name":"LED Bar 2","start":)" << config_.bar2_start << R"(,"segments":8,"enabled":true}])"
        << R"(,"moving_heads":[)"
        << R"({"name":"MH 1","start":51,"channels":11,"enabled":true},)"
        << R"({"name":"MH 2","start":62,"channels":11,"enabled":true},)"
        << R"({"name":"MH 3","start":73,"channels":11,"enabled":true},)"
        << R"({"name":"MH 4","start":84,"channels":11,"enabled":true}])"
        << R"(,"strobe":{"name":"Stairville 1500W Strobe","start":1,"channels":2,"enabled":true,"armed":)"
        << json_bool(strobe_armed_) << R"(,"beat_pulse":)" << json_bool(strobe_beat_pulse_)
        << R"(,"master":)" << strobe_master_ << R"(,"speed":)" << strobe_speed_
        << R"(},"fog":{"name":"unused","start":1,"channels":1,"enabled":false,"armed":false}}})";

    out << R"(,"effects":)" << rgb_scenes_.effects_json();
    out << R"(,"motion_modes":{"auto":"Auto","center_pulse":"Mitte Pulse","point_chase":"Punkt Chase","line_sweep":"Links/Rechts Sweep","depth_sweep":"Vorne/Hinten Sweep","cross_pairs":"2 Links / 2 Rechts","split_strobe":"Links/Rechts Strobe","x_cross":"X Cross","color_fan":"Color Fan","pair_random":"Paare Random"})";
    out << R"(,"motion_scenes":{"beat_drive":"Beat Drive","center_pulse":"Mitte Pulse","point_chase":"Punkt Chase","line_sweep":"Links/Rechts Sweep","depth_sweep":"Vorne/Hinten Sweep","cross_pairs":"2 Links / 2 Rechts","split_strobe":"Links/Rechts Strobe","x_cross":"X Cross","color_fan":"Color Fan","pair_random":"Paare Random"})";
    out << R"(,"presets":["lounge","club","rave","game_show","rgb_hard","custom"],"show":{},"preview":[)";
    for (std::size_t index = 0; index < preview_.size(); ++index) {
        if (index != 0) {
            out << ',';
        }
        const Rgb color = preview_.at(index);
        out << R"({"r":)" << static_cast<int>(color.r)
            << R"(,"g":)" << static_cast<int>(color.g)
            << R"(,"b":)" << static_cast<int>(color.b) << '}';
    }
    out << "]}";
    return out.str();
}

void SimpleEngine::mark_artnet_packet_sent() {
    std::lock_guard lock{mutex_};
    ++artnet_packets_;
}

void SimpleEngine::apply_preset_locked(const std::string& preset) {
    preset_ = preset;
    if (preset == "rave") {
        mood_ = 88;
        active_effects_ = {"rgb_beat_pulse", "rgb_chase", "rgb_comet", "rgb_spark"};
        active_scenes_ = {"beat_drive", "color_fan", "pair_random", "split_strobe", "x_cross"};
    } else if (preset == "rgb_hard") {
        mood_ = 78;
        active_effects_ = {"rgb_beat_pulse", "rgb_chase", "rgb_spark"};
        active_scenes_ = {"beat_drive", "gobo_chase", "side_pingpong", "split_strobe", "x_cross"};
    } else if (preset == "lounge") {
        mood_ = 35;
        active_effects_ = {"rgb_static", "rgb_beat_pulse"};
        active_scenes_ = {"beat_drive", "center_pulse"};
    } else if (preset == "game_show") {
        mood_ = 66;
        active_effects_ = {"rgb_static", "rgb_chase", "rgb_spark"};
        active_scenes_ = {"beat_drive", "cross_pairs", "line_sweep"};
    } else {
        mood_ = 58;
        active_effects_ = {"rgb_static", "rgb_beat_pulse", "rgb_comet"};
        active_scenes_ = {"beat_drive", "center_pulse", "cross_pairs", "depth_sweep", "line_sweep", "point_chase"};
        preset_ = "club";
    }
}

}  // namespace lightengine
