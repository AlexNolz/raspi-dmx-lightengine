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

void wheel_slots_json(std::ostringstream& out, const std::vector<FixtureWheelSlot>& slots) {
    out << '{';
    for (std::size_t index = 0; index < slots.size(); ++index) {
        if (index != 0U) {
            out << ',';
        }
        out << '"' << slots.at(index).id << R"(":")" << slots.at(index).label << '"';
    }
    out << '}';
}

}  // namespace

SimpleEngine::SimpleEngine(SimpleEngineConfig config)
    : config_{std::move(config)},
      preview_(static_cast<std::size_t>(config_.segments_per_bar) * 2U),
      bar1_{DmxAddress{config_.bar1_start}, config_.segments_per_bar},
      bar2_{DmxAddress{config_.bar2_start}, config_.segments_per_bar} {
    rgb_scenes_.load_palettes_from_file("shows/color_palettes.json");
    rgb_scenes_.load_scene_definitions_from_file("shows/rgb_scenes.json");
    moving_head_profile_ = Zkymzl11Profile::load_from_file("fixtures/zkymzl_11ch_moving_head.json");
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
                const bool phrase_boundary = typed_event.position >= 0 && typed_event.position % 16 == 0 &&
                    typed_event.position != last_effect_change_position_;
                if (typed_event.changed || phrase_boundary) {
                    select_next_effect_locked();
                    last_effect_change_position_ = typed_event.position;
                }
            } else if constexpr (std::is_same_v<Event, Os2lButtonEvent>) {
                if (typed_event.name == "blackout") {
                    blackout_held_ = typed_event.pressed;
                } else if (typed_event.name == "whiteout") {
                    whiteout_held_ = typed_event.pressed;
                } else if (typed_event.name == "colorstrobe") {
                    color_strobe_held_ = typed_event.pressed;
                } else if (typed_event.name == "strobeout") {
                    strobe_out_held_ = typed_event.pressed;
                } else if (typed_event.name == "next" && typed_event.pressed) {
                    select_next_effect_locked();
                } else if (typed_event.name == "fog" && typed_event.pressed && fog_armed_) {
                    fog_until_ = received_at + std::chrono::milliseconds{1200};
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
                } else if (typed_command.layer == LayerId::fog) {
                    fog_layer_enabled_ = typed_command.enabled;
                }
            } else if constexpr (std::is_same_v<Command, SetFixtureArmedCommand>) {
                if (typed_command.fixture == ArmedFixtureId::strobe) {
                    strobe_armed_ = typed_command.armed;
                } else {
                    fog_armed_ = typed_command.armed;
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
                if (active_effects_.empty()) {
                    active_effects_.push_back("rgb_static");
                }
                if (std::find(active_effects_.begin(), active_effects_.end(), selected_effect_) == active_effects_.end()) {
                    selected_effect_ = active_effects_.front();
                }
                preset_ = "custom";
            } else if constexpr (std::is_same_v<Command, SetMotionModeCommand>) {
                motion_mode_ = typed_command.motion_mode;
            } else if constexpr (std::is_same_v<Command, ToggleMotionSceneCommand>) {
                set_enabled(active_scenes_, typed_command.scene, typed_command.enabled);
                preset_ = "custom";
            } else if constexpr (std::is_same_v<Command, SetGoboControlCommand>) {
                gobo_enabled_ = typed_command.enabled;
                gobo_mode_ = typed_command.mode;
                selected_gobo_ = typed_command.selected_gobo;
                gobo_highpoint_only_ = typed_command.highpoint_only;
                gobo_shake_enabled_ = typed_command.shake_enabled;
                gobo_shake_mood_threshold_ = typed_command.shake_mood_threshold;
                preset_ = "custom";
            } else if constexpr (std::is_same_v<Command, SetColorWheelCommand>) {
                manual_color_enabled_ = typed_command.enabled;
                manual_color_use_raw_ = typed_command.use_raw_value;
                selected_color_ = typed_command.selected_color;
                manual_color_value_ = static_cast<std::uint8_t>(std::clamp(
                    static_cast<int>(typed_command.raw_value),
                    static_cast<int>(moving_head_profile_.color_test_min),
                    static_cast<int>(moving_head_profile_.color_test_max)));
            } else if constexpr (std::is_same_v<Command, SetArtNetCommand>) {
                config_.artnet_host = typed_command.host.empty() ? std::string{"127.0.0.1"} : typed_command.host;
                config_.artnet_universe = typed_command.universe;
                config_.segments_per_bar = static_cast<std::uint8_t>(std::max(1U, static_cast<unsigned>(typed_command.segment_count) / 2U));
                const auto total_channels = static_cast<std::uint16_t>(config_.segments_per_bar * 2U * 3U);
                const auto max_start = static_cast<std::uint16_t>(dmx_channel_count - total_channels + 1U);
                config_.bar1_start = std::min(typed_command.led_start_channel, max_start);
                config_.bar2_start = static_cast<std::uint16_t>(config_.bar1_start + config_.segments_per_bar * 3U);
                preview_.assign(static_cast<std::size_t>(config_.segments_per_bar) * 2U, Rgb{});
                bar1_ = RgbWashBar{DmxAddress{config_.bar1_start}, config_.segments_per_bar};
                bar2_ = RgbWashBar{DmxAddress{config_.bar2_start}, config_.segments_per_bar};
            } else if constexpr (std::is_same_v<Command, SetFixtureAddressCommand>) {
                if (typed_command.fixture == PatchFixtureId::led_bars && typed_command.index < 2U) {
                    if (typed_command.index == 0U) {
                        config_.bar1_start = typed_command.start;
                        bar1_ = RgbWashBar{DmxAddress{config_.bar1_start}, config_.segments_per_bar};
                    } else {
                        config_.bar2_start = typed_command.start;
                        bar2_ = RgbWashBar{DmxAddress{config_.bar2_start}, config_.segments_per_bar};
                    }
                } else if (typed_command.fixture == PatchFixtureId::moving_heads && typed_command.index < moving_head_starts_.size()) {
                    moving_head_starts_.at(typed_command.index) = typed_command.start;
                } else if (typed_command.fixture == PatchFixtureId::strobe) {
                    strobe_start_ = typed_command.start;
                } else if (typed_command.fixture == PatchFixtureId::fog) {
                    fog_start_ = typed_command.start;
                }
            } else if constexpr (std::is_same_v<Command, TriggerCommand>) {
                const auto now = std::chrono::steady_clock::now();
                const double default_seconds = typed_command.trigger == LiveTriggerId::whiteout ? 0.8
                    : typed_command.trigger == LiveTriggerId::fog ? 1.2
                                                                  : 4.0;
                const auto duration = std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                    std::chrono::duration<double>{typed_command.seconds > 0.0 ? typed_command.seconds : default_seconds});
                if (typed_command.trigger == LiveTriggerId::next) {
                    select_next_effect_locked();
                } else if (typed_command.trigger == LiveTriggerId::whiteout) {
                    whiteout_until_ = now + duration;
                } else if (typed_command.trigger == LiveTriggerId::color_strobe) {
                    color_strobe_until_ = now + duration;
                } else if (typed_command.trigger == LiveTriggerId::strobe_out) {
                    strobe_out_until_ = now + duration;
                } else if (typed_command.trigger == LiveTriggerId::fog && fog_armed_) {
                    fog_until_ = now + duration;
                } else if (typed_command.trigger == LiveTriggerId::blackout) {
                    blackout_ = !blackout_;
                }
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

    const BeatSnapshot beat = beat_clock_.snapshot(now);
    if (!running_ || blackout_ || blackout_held_) {
        render_safe_moving_head_blackout(frame);
        return frame;
    }

    render_auxiliary_fixtures(frame, beat, now);
    if (!led_layer_enabled_) {
        if (motion_layer_enabled_) {
            render_moving_heads(frame, beat);
        } else {
            render_safe_moving_head_blackout(frame);
        }
        return frame;
    }
    const RgbSceneContext scene_context{
        beat,
        clamp01(master_ * led_master_),
        static_cast<double>(mood_) / 100.0,
    };
    const RgbPalette& palette = rgb_scenes_.palette_for_preset(preset_, beat.position / 16);
    const std::vector<std::string> selected{selected_effect_};
    rgb_scenes_.render(bar1_, selected, scene_context, palette);
    rgb_scenes_.render(bar2_, selected, scene_context, palette);
    const bool whiteout_active = whiteout_held_ || now < whiteout_until_;
    const bool strobe_out_active = strobe_out_held_ || now < strobe_out_until_;
    const bool color_strobe_active = color_strobe_held_ || now < color_strobe_until_;
    if (whiteout_active) {
        bar1_.set_all(Rgb{to_dmx(scene_context.master), to_dmx(scene_context.master), to_dmx(scene_context.master)});
        bar2_.set_all(Rgb{to_dmx(scene_context.master), to_dmx(scene_context.master), to_dmx(scene_context.master)});
    } else if (strobe_out_active) {
        const double hz = 1.5 + strobe_speed_ * 12.5;
        const double seconds = std::chrono::duration<double>(now.time_since_epoch()).count();
        const bool on = static_cast<std::int64_t>(std::floor(seconds * hz * 2.0)) % 2 == 0;
        const std::uint8_t value = on ? to_dmx(scene_context.master * strobe_master_) : 0;
        bar1_.set_all(Rgb{value, value, value});
        bar2_.set_all(Rgb{value, value, value});
    } else if (color_strobe_active) {
        const auto beat_index = static_cast<std::size_t>(std::floor(beat.beat * 4.0));
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
    if (motion_layer_enabled_) {
        render_moving_heads(frame, beat);
    } else {
        render_safe_moving_head_blackout(frame);
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
        << R"(,"active_effect":")" << selected_effect_
        << R"(","active_effect_label":")" << active_effect_label_locked()
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
        << R"(","motion_mode":")" << motion_mode_ << '"'
        << R"(,"gobo":{"enabled":)" << json_bool(gobo_enabled_)
        << R"(,"mode":")" << gobo_mode_
        << R"(","selected_gobo":")" << selected_gobo_
        << R"(","highpoint_only":)" << json_bool(gobo_highpoint_only_)
        << R"(,"shake_enabled":)" << json_bool(gobo_shake_enabled_)
        << R"(,"shake_mood_threshold":)" << gobo_shake_mood_threshold_
        << R"(})"
        << R"(,"color_wheel":{"enabled":)" << json_bool(manual_color_enabled_)
        << R"(,"use_raw_value":)" << json_bool(manual_color_use_raw_)
        << R"(,"selected_color":")" << selected_color_
        << R"(","raw_value":)" << static_cast<int>(manual_color_value_)
        << R"(,"test_min":)" << static_cast<int>(moving_head_profile_.color_test_min)
        << R"(,"test_max":)" << static_cast<int>(moving_head_profile_.color_test_max)
        << R"(,"test_step":)" << static_cast<int>(moving_head_profile_.color_test_step)
        << R"(})"
        << R"(,"enabled_effects":)";
    json_string_array(out, active_effects_);
    out << R"(,"enabled_motion_scenes":)";
    json_string_array(out, active_scenes_);
    out << R"(,"layers":{"led_bars":)" << json_bool(led_layer_enabled_)
        << R"(,"motion":)" << json_bool(motion_layer_enabled_)
        << R"(,"strobe":)" << json_bool(strobe_armed_)
        << R"(,"fog":)" << json_bool(fog_layer_enabled_) << "}";
    out << R"(,"fixtures":{"led_bars":[)"
        << R"({"name":"LED Bar 1","start":)" << config_.bar1_start << R"(,"segments":8,"enabled":true},)"
        << R"({"name":"LED Bar 2","start":)" << config_.bar2_start << R"(,"segments":8,"enabled":true}])"
        << R"(,"moving_heads":[)"
        << R"({"name":"MH 1","start":)" << moving_head_starts_.at(0) << R"(,"channels":11,"enabled":true},)"
        << R"({"name":"MH 2","start":)" << moving_head_starts_.at(1) << R"(,"channels":11,"enabled":true},)"
        << R"({"name":"MH 3","start":)" << moving_head_starts_.at(2) << R"(,"channels":11,"enabled":true},)"
        << R"({"name":"MH 4","start":)" << moving_head_starts_.at(3) << R"(,"channels":11,"enabled":true}])"
        << R"(,"strobe":{"name":"Stairville 1500W Strobe","start":)" << strobe_start_ << R"(,"channels":2,"enabled":true,"armed":)"
        << json_bool(strobe_armed_) << R"(,"beat_pulse":)" << json_bool(strobe_beat_pulse_)
        << R"(,"master":)" << strobe_master_ << R"(,"speed":)" << strobe_speed_
        << R"(},"fog":{"name":"Stairville AF-40 DMX Fog","start":)" << fog_start_
        << R"(,"channels":1,"enabled":true,"armed":)" << json_bool(fog_armed_) << "}}}";

    out << R"(,"effects":)" << rgb_scenes_.effects_json();
    out << R"(,"motion_modes":{"auto":"Auto","center":"Mitte / Gobo-Test","center_pulse":"Mitte Pulse","point_chase":"Punkt Chase","line_sweep":"Links/Rechts Sweep","depth_sweep":"Vorne/Hinten Sweep","cross_pairs":"2 Links / 2 Rechts","split_strobe":"Links/Rechts Strobe","x_cross":"X Cross","color_fan":"Color Fan","pair_random":"Paare Random"})";
    out << R"(,"motion_scenes":{"mh_center_pulse":"MH Center Pulse","mh_cross_sweep":"MH Cross Sweep","mh_rave_hits":"MH Rave Hits"})";
    out << R"(,"gobo_modes":{"static":"Manuell","beat_step":"Beat Step","random_beat":"Zufällig auf Beat","phrase_random":"Zufällig pro Phrase"})";
    out << R"(,"gobos":)";
    wheel_slots_json(out, moving_head_profile_.gobos);
    out << R"(,"moving_head_colors":)";
    wheel_slots_json(out, moving_head_profile_.colors);
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

ArtNetEndpoint SimpleEngine::artnet_endpoint() const {
    std::lock_guard lock{mutex_};
    return ArtNetEndpoint{
        config_.artnet_host,
        6454,
        ArtNetUniverse{config_.artnet_universe},
    };
}

bool SimpleEngine::output_active() const {
    std::lock_guard lock{mutex_};
    return running_;
}

void SimpleEngine::render_safe_moving_head_blackout(DmxFrame& frame) const {
    Zkymzl11Look park;
    park.color_wheel = moving_head_profile_.color_value("white").value_or(moving_head_profile_.color_test_default);
    park.gobo = moving_head_profile_.gobo_value("open").value_or(0);
    for (const std::uint16_t start_address : moving_head_starts_) {
        if (start_address > dmx_channel_count - 10U) {
            continue;
        }
        Zkymzl11MovingHead{DmxAddress{start_address}}.render_to(frame, park);
    }
}

void SimpleEngine::render_moving_heads(DmxFrame& frame, const BeatSnapshot& beat) const {
    const double mood = static_cast<double>(mood_) / 100.0;
    const double beat_hit = std::exp(-beat.phase * (3.0 + mood * 6.0));

    std::string scene = motion_mode_;
    if (scene == "auto") {
        if (active_scenes_.empty()) {
            scene = "mh_center_pulse";
        } else {
            const auto phrase = static_cast<std::size_t>(std::max<std::int64_t>(0, beat.position) / 16);
            scene = active_scenes_.at(phrase % active_scenes_.size());
        }
    }

    for (std::size_t index = 0; index < moving_head_starts_.size(); ++index) {
        const std::uint16_t start_address = moving_head_starts_.at(index);
        if (start_address > dmx_channel_count - 10U) {
            continue;
        }

        Zkymzl11Look look;
        double level = 0.55 + mood * 0.30;
        if (scene == "mh_cross_sweep" || scene == "line_sweep" || scene == "x_cross") {
            const double wave = std::sin(beat.beat * (0.28 + mood * 0.32) + static_cast<double>(index) * 1.5707963268);
            look.pan = static_cast<std::uint8_t>(std::lround(85.0 + wave * 45.0));
            look.tilt = static_cast<std::uint8_t>(179 + (index % 2U == 0U ? -10 : 10));
            level = 0.62 + beat_hit * 0.25;
        } else if (scene == "mh_rave_hits" || scene == "split_strobe" || scene == "pair_random") {
            const auto step = static_cast<std::size_t>(std::max<std::int64_t>(0, static_cast<std::int64_t>(std::floor(beat.beat))));
            const bool right = (step + index / 2U) % 2U != 0U;
            look.pan = right ? 125 : 45;
            look.tilt = static_cast<std::uint8_t>(index % 2U == 0U ? 165 : 193);
            level = 0.32 + beat_hit * 0.68;
        } else if (scene == "center") {
            look.pan = 85;
            look.tilt = 179;
            level = 0.72;
        } else {
            look.pan = 85;
            look.tilt = 179;
            level = 0.42 + beat_hit * 0.48;
        }

        const auto color_step = static_cast<std::size_t>(std::max<std::int64_t>(0, beat.position) / 8);
        look.color_wheel = moving_head_profile_.colors.at(
            (color_step + index / 2U) % moving_head_profile_.colors.size()).value;
        if (manual_color_enabled_) {
            look.color_wheel = manual_color_use_raw_
                ? manual_color_value_
                : moving_head_profile_.color_value(selected_color_).value_or(look.color_wheel);
        }

        std::uint8_t selected_gobo = moving_head_profile_.gobo_value("open").value_or(0);
        if (gobo_enabled_) {
            if (gobo_mode_ == "static") {
                selected_gobo = moving_head_profile_.gobo_value(selected_gobo_).value_or(selected_gobo);
            } else if (gobo_mode_ == "phrase_random") {
                const auto phrase = static_cast<std::size_t>(std::max<std::int64_t>(0, beat.position) / 16);
                selected_gobo = moving_head_profile_.gobos.at(
                    (phrase * 5U + index * 3U) % moving_head_profile_.gobos.size()).value;
            } else if (gobo_mode_ == "random_beat") {
                const auto step = static_cast<std::size_t>(std::max<std::int64_t>(0, beat.position));
                selected_gobo = moving_head_profile_.gobos.at(
                    (step * 5U + index * 3U + 1U) % moving_head_profile_.gobos.size()).value;
            } else {
                const auto step = static_cast<std::size_t>(std::max<std::int64_t>(0, beat.position) / 2);
                selected_gobo = moving_head_profile_.gobos.at((step + index) % moving_head_profile_.gobos.size()).value;
            }
            const bool highpoint = beat.strength >= 0.75 || (beat.position % 16 >= 0 && beat.position % 16 < 4);
            if (gobo_highpoint_only_ && !highpoint) {
                selected_gobo = moving_head_profile_.gobo_value("open").value_or(0);
            }
            const std::uint8_t open_gobo = moving_head_profile_.gobo_value("open").value_or(0);
            if (gobo_shake_enabled_ && selected_gobo != open_gobo && mood >= gobo_shake_mood_threshold_) {
                selected_gobo = static_cast<std::uint8_t>(std::min(127, static_cast<int>(selected_gobo) + 64));
            }
        }
        look.gobo = selected_gobo;
        look.dimmer = to_dmx(clamp01(level * master_ * motion_master_));
        look.shutter = look.dimmer > 0U ? 10 : 0;
        look.movement_speed = static_cast<std::uint8_t>(std::lround(210.0 - mood * 120.0));
        Zkymzl11MovingHead{DmxAddress{start_address}}.render_to(frame, look);
    }
}

void SimpleEngine::render_auxiliary_fixtures(
    DmxFrame& frame,
    const BeatSnapshot& beat,
    const std::chrono::steady_clock::time_point now) const {
    const bool strobe_out_active = strobe_out_held_ || now < strobe_out_until_;
    const bool beat_pulse_active = strobe_beat_pulse_ && beat.phase < 0.14;
    if (strobe_armed_ && strobe_start_ < dmx_channel_count) {
        const std::size_t start = DmxAddress{strobe_start_}.zero_based();
        if (strobe_out_active || beat_pulse_active) {
            frame.at(start) = to_dmx(strobe_speed_);
            frame.at(start + 1U) = to_dmx(strobe_master_ * master_);
        }
    }

    if (fog_layer_enabled_ && fog_armed_ && now < fog_until_ && fog_start_ <= dmx_channel_count) {
        frame.at(DmxAddress{fog_start_}.zero_based()) = 255;
    }
}

void SimpleEngine::select_next_effect_locked() {
    if (active_effects_.empty()) {
        active_effects_.push_back("rgb_static");
    }
    const auto current = std::find(active_effects_.begin(), active_effects_.end(), selected_effect_);
    if (current == active_effects_.end() || std::next(current) == active_effects_.end()) {
        selected_effect_ = active_effects_.front();
    } else {
        selected_effect_ = *std::next(current);
    }
}

std::string SimpleEngine::active_effect_label_locked() const {
    const auto definition = std::find_if(
        rgb_scenes_.scene_definitions().begin(),
        rgb_scenes_.scene_definitions().end(),
        [&](const RgbSceneDefinition& scene) { return scene.id == selected_effect_; });
    return definition == rgb_scenes_.scene_definitions().end() ? selected_effect_ : definition->label;
}

void SimpleEngine::apply_preset_locked(const std::string& preset) {
    if (preset == "custom") {
        preset_ = "custom";
        return;
    }
    preset_ = preset;
    if (preset == "rave") {
        mood_ = 88;
        active_effects_ = {"rgb_beat_pulse", "rgb_chase", "rgb_comet", "rgb_spark"};
        active_scenes_ = {"mh_cross_sweep", "mh_rave_hits"};
        gobo_enabled_ = true;
        gobo_mode_ = "random_beat";
        gobo_highpoint_only_ = false;
        gobo_shake_enabled_ = true;
    } else if (preset == "rgb_hard") {
        mood_ = 78;
        active_effects_ = {"rgb_beat_pulse", "rgb_chase", "rgb_spark"};
        active_scenes_ = {"mh_cross_sweep", "mh_rave_hits"};
        gobo_enabled_ = true;
        gobo_mode_ = "beat_step";
        gobo_highpoint_only_ = false;
        gobo_shake_enabled_ = true;
    } else if (preset == "lounge") {
        mood_ = 35;
        active_effects_ = {"rgb_static", "rgb_beat_pulse"};
        active_scenes_ = {"mh_center_pulse"};
        gobo_enabled_ = false;
        gobo_mode_ = "static";
        gobo_highpoint_only_ = false;
        gobo_shake_enabled_ = false;
    } else if (preset == "game_show") {
        mood_ = 66;
        active_effects_ = {"rgb_static", "rgb_chase", "rgb_spark"};
        active_scenes_ = {"mh_center_pulse", "mh_cross_sweep"};
        gobo_enabled_ = true;
        gobo_mode_ = "phrase_random";
        gobo_highpoint_only_ = true;
        gobo_shake_enabled_ = false;
    } else {
        mood_ = 58;
        active_effects_ = {"rgb_static", "rgb_beat_pulse", "rgb_comet"};
        active_scenes_ = {"mh_center_pulse", "mh_cross_sweep"};
        gobo_enabled_ = false;
        gobo_mode_ = "beat_step";
        gobo_highpoint_only_ = false;
        gobo_shake_enabled_ = false;
        preset_ = "club";
    }
    selected_effect_ = active_effects_.front();
}

}  // namespace lightengine
