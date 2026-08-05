#include "lightengine/artnet_sender.hpp"
#include "lightengine/control_command.hpp"
#include "lightengine/engine_input.hpp"
#include "lightengine/fixture_runtime.hpp"
#include "lightengine/os2l_event.hpp"
#include "lightengine/motion_scene.hpp"
#include "lightengine/music_dynamics.hpp"
#include "lightengine/project.hpp"
#include "lightengine/rgb_scene.hpp"
#include "lightengine/show_layers.hpp"
#include "lightengine/simple_engine.hpp"

#include <algorithm>
#include <iostream>
#include <chrono>
#include <optional>
#include <string>
#include <variant>

int main() {
    {
        lightengine::DmxFrame frame{};
        frame.at(0) = 255;
        frame.at(511) = 127;

        const lightengine::ArtDmxPacket packet{lightengine::ArtNetUniverse{3}, 42, frame};
        const auto& bytes = packet.bytes();
        if (bytes.size() != lightengine::ArtDmxPacket::header_size + lightengine::dmx_channel_count) {
            std::cerr << "ArtDMX packet has unexpected size\n";
            return 1;
        }
        if (std::string{reinterpret_cast<const char*>(bytes.data()), 8} != std::string{"Art-Net\0", 8}) {
            std::cerr << "ArtDMX packet has invalid ID\n";
            return 1;
        }
        if (bytes.at(8) != 0x00 || bytes.at(9) != 0x50) {
            std::cerr << "ArtDMX packet has invalid opcode\n";
            return 1;
        }
        if (bytes.at(12) != 42 || bytes.at(14) != 3 || bytes.at(15) != 0) {
            std::cerr << "ArtDMX packet has invalid sequence or universe\n";
            return 1;
        }
        if (bytes.at(16) != 2 || bytes.at(17) != 0) {
            std::cerr << "ArtDMX packet has invalid DMX length\n";
            return 1;
        }
        if (bytes.at(18) != 255 || bytes.back() != 127) {
            std::cerr << "ArtDMX packet did not copy DMX data\n";
            return 1;
        }
    }

    {
        const std::optional<lightengine::Os2lEvent> event = lightengine::parse_os2l_event(
            R"({"evt":"beat","change":false,"pos":16,"bpm":116.04,"strength":0.8})");
        if (!event || !std::holds_alternative<lightengine::Os2lBeatEvent>(*event)) {
            std::cerr << "OS2L beat event was not parsed\n";
            return 1;
        }
        const auto beat = std::get<lightengine::Os2lBeatEvent>(*event);
        if (beat.position != 16 || beat.bpm != 116.04 || beat.strength != 0.8 || beat.changed || !beat.strength_available) {
            std::cerr << "OS2L beat event contains wrong values\n";
            return 1;
        }
    }

    {
        const auto event = lightengine::parse_os2l_event(R"({"evt":"beat","pos":17,"bpm":128})");
        if (!event || !std::holds_alternative<lightengine::Os2lBeatEvent>(*event)) {
            std::cerr << "OS2L beat without strength was not parsed\n";
            return 1;
        }
        const auto beat = std::get<lightengine::Os2lBeatEvent>(*event);
        if (beat.strength_available || beat.strength != 0.5) {
            std::cerr << "Missing OS2L strength was incorrectly treated as maximum intensity\n";
            return 1;
        }
    }

    {
        const std::optional<lightengine::Os2lEvent> event =
            lightengine::parse_os2l_event(R"({"evt":"btn","name":"color_strobe","state":"off"})");
        if (!event || !std::holds_alternative<lightengine::Os2lButtonEvent>(*event)) {
            std::cerr << "OS2L button event was not parsed\n";
            return 1;
        }
        const auto button = std::get<lightengine::Os2lButtonEvent>(*event);
        if (button.name != "colorstrobe" || button.pressed) {
            std::cerr << "OS2L button event contains wrong values\n";
            return 1;
        }
    }

    {
        const std::optional<lightengine::Os2lEvent> event =
            lightengine::parse_os2l_event(R"({"evt":"cmd","id":2,"param":100})");
        if (!event || !std::holds_alternative<lightengine::Os2lCommandEvent>(*event)) {
            std::cerr << "OS2L command event was not parsed\n";
            return 1;
        }
        const auto command = std::get<lightengine::Os2lCommandEvent>(*event);
        if (command.id != 2 || command.parameter != 100.0) {
            std::cerr << "OS2L command event contains wrong values\n";
            return 1;
        }
    }

    {
        if (lightengine::normalize_os2l_button_name("Color Strobe!") != "colorstrobe") {
            std::cerr << "OS2L button name was not normalized\n";
            return 1;
        }
    }

    {
        lightengine::DmxFrame frame{};
        lightengine::RgbWash wash{lightengine::DmxAddress{10}, lightengine::DmxAddress{11}, lightengine::DmxAddress{12}};
        wash.set_color(lightengine::Rgb{1, 2, 3});
        wash.render_to(frame);
        if (frame.at(9) != 1 || frame.at(10) != 2 || frame.at(11) != 3) {
            std::cerr << "RgbWash did not render to the expected DMX channels\n";
            return 1;
        }
    }

    {
        lightengine::DmxFrame frame{};
        lightengine::RgbWashBar bar{lightengine::DmxAddress{3}, 8};
        bar.set_wash(0, lightengine::Rgb{10, 20, 30});
        bar.set_wash(1, lightengine::Rgb{40, 50, 60});
        bar.render_to(frame);
        if (frame.at(2) != 10 || frame.at(3) != 20 || frame.at(4) != 30) {
            std::cerr << "RgbWashBar did not render first wash to the expected DMX channels\n";
            return 1;
        }
        if (frame.at(5) != 40 || frame.at(6) != 50 || frame.at(7) != 60) {
            std::cerr << "RgbWashBar did not render second wash to the expected DMX channels\n";
            return 1;
        }
    }

    {
        lightengine::DmxFrame frame{};
        lightengine::RgbPar par{lightengine::DmxAddress{100}};
        par.render_to(frame, lightengine::RgbParLook{200, lightengine::Rgb{10, 20, 30}});
        if (frame.at(99) != 200 || frame.at(100) != 10 || frame.at(101) != 20 || frame.at(102) != 30 ||
            frame.at(103) != 0 || frame.at(104) != 0 || frame.at(105) != 0) {
            std::cerr << "RgbPar did not isolate master and RGB from the unused channels\n";
            return 1;
        }
    }

    {
        lightengine::DmxFrame frame{};
        lightengine::Zkymzl11MovingHead head{lightengine::DmxAddress{51}};
        head.render_to(frame, lightengine::Zkymzl11Look{85, 179, 11, 26, 10, 160, 140, 0});
        if (frame.at(50) != 85 || frame.at(51) != 0 || frame.at(52) != 179 || frame.at(54) != 11 ||
            frame.at(55) != 26 || frame.at(56) != 10 || frame.at(57) != 160 || frame.at(58) != 140 ||
            frame.at(59) != 0) {
            std::cerr << "Zkymzl11MovingHead rendered an incorrect channel mapping\n";
            return 1;
        }
    }

    {
        const std::optional<lightengine::ControlCommand> command =
            lightengine::parse_control_command(R"({"action":"set_mood","mood":88,"custom":true})");
        if (!command || !std::holds_alternative<lightengine::SetMoodCommand>(*command)) {
            std::cerr << "set_mood command was not parsed\n";
            return 1;
        }
        const auto mood = std::get<lightengine::SetMoodCommand>(*command);
        if (mood.mood != 88 || !mood.mark_custom) {
            std::cerr << "set_mood command contains wrong values\n";
            return 1;
        }
    }

    {
        const std::optional<lightengine::ControlCommand> command =
            lightengine::parse_control_command(R"({"action":"set_output_master","target":"motion_master","value":0.42})");
        if (!command || !std::holds_alternative<lightengine::SetOutputMasterCommand>(*command)) {
            std::cerr << "set_output_master command was not parsed\n";
            return 1;
        }
        const auto output_master = std::get<lightengine::SetOutputMasterCommand>(*command);
        if (output_master.target != lightengine::OutputMasterTarget::motion || output_master.value != 0.42) {
            std::cerr << "set_output_master command contains wrong values\n";
            return 1;
        }
    }

    {
        const std::optional<lightengine::ControlCommand> command =
            lightengine::parse_control_command(R"({"action":"set_layer","layer":"fog","enabled":true})");
        if (!command || !std::holds_alternative<lightengine::SetLayerCommand>(*command)) {
            std::cerr << "set_layer command was not parsed\n";
            return 1;
        }
        const auto layer = std::get<lightengine::SetLayerCommand>(*command);
        if (layer.layer != lightengine::LayerId::fog || !layer.enabled) {
            std::cerr << "set_layer command contains wrong values\n";
            return 1;
        }
    }

    {
        const auto command = lightengine::parse_control_command(
            R"({"action":"set_beat_pulse","target":"motion","enabled":false})");
        if (!command || !std::holds_alternative<lightengine::SetBeatPulseCommand>(*command)) {
            std::cerr << "set_beat_pulse command was not parsed\n";
            return 1;
        }
        const auto pulse = std::get<lightengine::SetBeatPulseCommand>(*command);
        if (pulse.target != lightengine::BeatPulseTarget::motion || pulse.enabled) {
            std::cerr << "set_beat_pulse command contains wrong values\n";
            return 1;
        }
    }

    {
        const std::optional<lightengine::ControlCommand> command =
            lightengine::parse_control_command(R"({"action":"set_hold_trigger","name":"blackout","held":true})");
        if (!command || !std::holds_alternative<lightengine::SetHoldTriggerCommand>(*command)) {
            std::cerr << "set_hold_trigger command was not parsed\n";
            return 1;
        }
        const auto hold = std::get<lightengine::SetHoldTriggerCommand>(*command);
        if (hold.trigger != lightengine::LiveTriggerId::blackout || !hold.held) {
            std::cerr << "set_hold_trigger command contains wrong values\n";
            return 1;
        }
    }

    {
        const std::optional<lightengine::ControlCommand> command = lightengine::parse_control_command(
            R"({"action":"set_artnet","artnet_host":"192.168.137.255","artnet_universe":0,"led_start_channel":3,"segment_count":16})");
        if (!command || !std::holds_alternative<lightengine::SetArtNetCommand>(*command)) {
            std::cerr << "set_artnet command was not parsed\n";
            return 1;
        }
        const auto artnet = std::get<lightengine::SetArtNetCommand>(*command);
        if (artnet.host != "192.168.137.255" || artnet.universe != 0 || artnet.led_start_channel != 3 || artnet.segment_count != 16) {
            std::cerr << "set_artnet command contains wrong values\n";
            return 1;
        }
    }

    {
        const std::optional<lightengine::ControlCommand> command =
            lightengine::parse_control_command(R"({"action":"set_fixture_address","fixture":"moving_heads","index":2,"start":73})");
        if (!command || !std::holds_alternative<lightengine::SetFixtureAddressCommand>(*command)) {
            std::cerr << "set_fixture_address command was not parsed\n";
            return 1;
        }
        const auto address = std::get<lightengine::SetFixtureAddressCommand>(*command);
        if (address.fixture != lightengine::PatchFixtureId::moving_heads || address.index != 2 || address.start != 73) {
            std::cerr << "set_fixture_address command contains wrong values\n";
            return 1;
        }
    }

    {
        const lightengine::Zkymzl11Profile profile =
            lightengine::Zkymzl11Profile::load_from_file("fixtures/zkymzl_11ch_moving_head.json");
        if (profile.gobo_value("spiral") != 0 || profile.gobo_value("open") != 18 ||
            profile.gobo_value("cloverleaf") != 26 || profile.color_value("white") != 8 ||
            profile.color_value("red") != 24 || profile.color_value("cyan") != 40 ||
            profile.color_value("amber") != 56 || profile.color_value("blue") != 72 ||
            profile.color_value("yellow") != 88 || profile.color_value("green") != 104 ||
            profile.color_value("magenta") != 120 || profile.color_test_max != 255 ||
            profile.pan_direction != -1.0 || profile.left_pan_scale != 0.72 ||
            profile.target_y_offset != 0.10) {
            std::cerr << "ZKYMZL wheel mappings were not loaded from the fixture JSON\n";
            return 1;
        }
        const lightengine::ShowLayerContext context{
            lightengine::BeatSnapshot{8.0, 0.0, 8, 112.0, 0.5, true, false},
            lightengine::MusicDynamicsSnapshot{lightengine::MusicalSection::calm, 0.35, 0.3, 0.0, false, false},
            0.4,
            "warmup",
            42U,
        };
        lightengine::GoboLayerRequest request;
        request.enabled = true;
        request.mode = "musical";
        const lightengine::GoboLayerSelection selected =
            lightengine::GoboLayer{}.resolve(profile, context, request, 0U, {"cloverleaf"});
        if (selected.value != 26U) {
            std::cerr << "Gobo layer did not respect its enabled-pattern pool\n";
            return 1;
        }
    }

    {
        const std::optional<lightengine::ControlCommand> command = lightengine::parse_control_command(
            R"({"action":"set_gobo_control","enabled":true,"mode":"random_beat","selected_gobo":"cloverleaf","highpoint_only":true,"shake_enabled":true,"shake_mood_threshold":0.74,"fast_peak_enabled":false})");
        if (!command || !std::holds_alternative<lightengine::SetGoboControlCommand>(*command)) {
            std::cerr << "set_gobo_control command was not parsed\n";
            return 1;
        }
        const auto gobo = std::get<lightengine::SetGoboControlCommand>(*command);
        if (!gobo.enabled || gobo.mode != "random_beat" || gobo.selected_gobo != "cloverleaf" || !gobo.highpoint_only || !gobo.shake_enabled ||
            gobo.shake_mood_threshold < 0.73 || gobo.shake_mood_threshold > 0.75 || gobo.fast_peak_enabled) {
            std::cerr << "set_gobo_control command contains wrong values\n";
            return 1;
        }
    }

    {
        const auto palette = lightengine::parse_control_command(
            R"({"action":"toggle_color_palette","palette":"sunset_gold","enabled":false})");
        const auto gobo = lightengine::parse_control_command(
            R"({"action":"toggle_gobo_pattern","gobo":"dot_grid","enabled":false})");
        const auto color_trigger = lightengine::parse_live_trigger_id("next_color");
        if (!palette || !std::holds_alternative<lightengine::ToggleColorPaletteCommand>(*palette) ||
            std::get<lightengine::ToggleColorPaletteCommand>(*palette).enabled ||
            !gobo || !std::holds_alternative<lightengine::ToggleGoboPatternCommand>(*gobo) ||
            std::get<lightengine::ToggleGoboPatternCommand>(*gobo).enabled ||
            color_trigger != lightengine::LiveTriggerId::next_color) {
            std::cerr << "Color-palette or gobo-pattern controls were not parsed\n";
            return 1;
        }
    }

    {
        const std::optional<lightengine::ControlCommand> command = lightengine::parse_control_command(
            R"({"action":"set_color_wheel","enabled":true,"use_raw_value":true,"selected_color":"cyan","raw_value":37})");
        if (!command || !std::holds_alternative<lightengine::SetColorWheelCommand>(*command)) {
            std::cerr << "set_color_wheel command was not parsed\n";
            return 1;
        }
        const auto color = std::get<lightengine::SetColorWheelCommand>(*command);
        if (!color.enabled || !color.use_raw_value || color.selected_color != "cyan" || color.raw_value != 37) {
            std::cerr << "set_color_wheel command contains wrong values\n";
            return 1;
        }
    }

    {
        const std::optional<lightengine::ControlCommand> command =
            lightengine::parse_control_command(R"({"action":"set_master","master":5})");
        if (!command || !std::holds_alternative<lightengine::SetMasterCommand>(*command)) {
            std::cerr << "set_master command was not parsed\n";
            return 1;
        }
        if (std::get<lightengine::SetMasterCommand>(*command).value != 1.0) {
            std::cerr << "set_master command was not clamped\n";
            return 1;
        }
    }

    {
        const auto now = std::chrono::steady_clock::now();
        const lightengine::EngineInput input = lightengine::make_engine_input(
            lightengine::Os2lBeatEvent{16, 116.04, 0.8, false},
            now);
        if (!std::holds_alternative<lightengine::EngineBeatInput>(input.payload) || input.received_at != now) {
            std::cerr << "Engine input did not keep beat payload and timestamp\n";
            return 1;
        }
    }

    {
        lightengine::BeatClock clock;
        const auto now = std::chrono::steady_clock::now();
        clock.on_beat(lightengine::Os2lBeatEvent{32, 120.0, 0.75, false, true}, now);
        const lightengine::BeatSnapshot snapshot = clock.snapshot(now + std::chrono::milliseconds{125});
        if (snapshot.position != 32 || snapshot.bpm != 120.0 || snapshot.strength != 0.75 ||
            !snapshot.locked_to_os2l || !snapshot.strength_available) {
            std::cerr << "BeatClock did not keep OS2L beat values\n";
            return 1;
        }
        if (snapshot.phase < 0.24 || snapshot.phase > 0.26) {
            std::cerr << "BeatClock phase is not synced to OS2L beat time\n";
            return 1;
        }
    }

    {
        lightengine::MusicDynamicsEstimator estimator;
        const lightengine::BeatSnapshot calm_beat{0.0, 0.0, 0, 120.0, 0.5, true, false};
        const lightengine::BeatSnapshot peak_beat{48.0, 0.0, 48, 140.0, 0.5, true, false};
        const auto calm = estimator.snapshot(calm_beat, 0.58, "club");
        const auto peak = estimator.snapshot(peak_beat, 0.88, "rave");
        if (calm.section != lightengine::MusicalSection::calm || peak.section != lightengine::MusicalSection::peak ||
            peak.energy <= calm.energy || calm.strength_available || !peak.phrase_boundary) {
            std::cerr << "MusicDynamicsEstimator did not produce a controlled phrase energy curve\n";
            return 1;
        }
    }

    {
        lightengine::RgbSceneMixer mixer;
        mixer.load_palettes_from_file("shows/color_palettes.json");
        mixer.load_scene_definitions_from_file("shows/rgb_scenes.json");
        lightengine::RgbWashBar bar{lightengine::DmxAddress{1}, 8};
        const lightengine::RgbSceneContext context{
            lightengine::BeatSnapshot{16.0, 0.0, 16, 120.0, 1.0, true},
            1.0,
            0.6,
        };
        mixer.render(bar, {"rgb_static", "rgb_beat_pulse", "rgb_comet"}, context, mixer.palette_for_preset("club"));
        bool has_output = false;
        for (std::size_t segment = 0; segment < bar.size(); ++segment) {
            const lightengine::Rgb color = bar.wash_color(segment);
            has_output = has_output || color.r != 0 || color.g != 0 || color.b != 0;
        }
        const std::string rave_palette = mixer.palette_for_preset("rave").id;
        if (!has_output || mixer.palette_by_id("club_blue_amber").colors.size() < 4 ||
            (rave_palette != "rave_neon" && rave_palette != "rave_acid" && rave_palette != "rgb_hard")) {
            std::cerr << "RgbSceneMixer did not render scenes or select palettes\n";
            return 1;
        }
        for (const lightengine::RgbSceneDefinition& definition : mixer.scene_definitions()) {
            lightengine::RgbWashBar scene_bar{lightengine::DmxAddress{1}, 8};
            mixer.render(scene_bar, {definition.id}, context, mixer.palette_for_preset("club"));
            bool scene_has_output = false;
            for (std::size_t segment = 0; segment < scene_bar.size(); ++segment) {
                const lightengine::Rgb color = scene_bar.wash_color(segment);
                scene_has_output = scene_has_output || color.r != 0 || color.g != 0 || color.b != 0;
            }
            if (!scene_has_output) {
                std::cerr << "RGB scene has no renderer or output: " << definition.id << '\n';
                return 1;
            }
        }
        lightengine::RgbWashBar grouped_bar{lightengine::DmxAddress{1}, 8};
        mixer.render(grouped_bar, {"rainbow"}, context, mixer.palette_for_preset("hardstyle"));
        for (std::size_t segment = 0; segment < grouped_bar.size(); segment += 2U) {
            const lightengine::Rgb first = grouped_bar.wash_color(segment);
            const lightengine::Rgb second = grouped_bar.wash_color(segment + 1U);
            if (first.r != second.r || first.g != second.g || first.b != second.b) {
                std::cerr << "RGB scenes did not keep adjacent LED segments in strong two-segment groups\n";
                return 1;
            }
        }

        const lightengine::ShowLayerContext calm_layers{
            lightengine::BeatSnapshot{1.0, 0.0, 1, 112.0, 0.5, true, false},
            lightengine::MusicDynamicsSnapshot{lightengine::MusicalSection::calm, 0.38, 0.35, 0.0, false, false},
            0.38,
            "warmup",
            12345U,
        };
        const lightengine::ColorLayer color_layer;
        const lightengine::ColorLayerSelection first_colors = color_layer.resolve(mixer, calm_layers);
        const lightengine::ColorLayerSelection repeated_colors = color_layer.resolve(mixer, calm_layers);
        if (first_colors.hold_beats != 64 || first_colors.palette.id != repeated_colors.palette.id ||
            first_colors.palette.color_names != repeated_colors.palette.color_names ||
            first_colors.palette.colors != repeated_colors.palette.colors) {
            std::cerr << "Color layer was not stable for a calm musical section\n";
            return 1;
        }

        lightengine::ShowLayerContext rgb_layers = calm_layers;
        rgb_layers.preset = "rgb_hard";
        const lightengine::ColorLayerSelection rgb_first = color_layer.resolve(mixer, rgb_layers, {"rgb_hard"});
        rgb_layers.color_nonce = 1U;
        const lightengine::ColorLayerSelection rgb_next = color_layer.resolve(mixer, rgb_layers, {"rgb_hard"});
        if (rgb_first.palette.color_names.size() != 3U ||
            std::find(rgb_first.palette.color_names.begin(), rgb_first.palette.color_names.end(), "red") == rgb_first.palette.color_names.end() ||
            std::find(rgb_first.palette.color_names.begin(), rgb_first.palette.color_names.end(), "blue") == rgb_first.palette.color_names.end() ||
            std::find(rgb_first.palette.color_names.begin(), rgb_first.palette.color_names.end(), "green") == rgb_first.palette.color_names.end() ||
            rgb_first.palette.color_names == rgb_next.palette.color_names) {
            std::cerr << "Pure RGB palette did not contain and rotate only red, blue and green: first=";
            for (const std::string& color : rgb_first.palette.color_names) std::cerr << color << ',';
            std::cerr << " next=";
            for (const std::string& color : rgb_next.palette.color_names) std::cerr << color << ',';
            std::cerr << " palettes=";
            for (const auto& palette : mixer.palettes()) std::cerr << palette.id << ':' << palette.color_names.size() << ',';
            std::cerr << '\n';
            return 1;
        }

        const lightengine::SceneLayerPlanner planner;
        const std::string calm_scene = planner.select_rgb_scene(
            {"strobe", "breathe"}, mixer.scene_definitions(), calm_layers);
        if (calm_scene != "breathe") {
            std::cerr << "Scene layer allowed a peak-only effect during a calm section\n";
            return 1;
        }
    }

    {
        lightengine::MotionSceneLibrary motions;
        motions.load_from_file("shows/moving_head_scenes.json");
        const lightengine::BeatSnapshot beat{16.0, 0.0, 16, 120.0, 1.0, true};
        if (motions.scenes().size() < 20U || motions.find("pair_random") == nullptr ||
            !motions.find("gobo_chase")->allow_shake || motions.find("gobo_chase")->energy_min < 0.5) {
            std::cerr << "Moving-head JSON scene library is incomplete\n";
            return 1;
        }
        for (const lightengine::MotionSceneDefinition& scene : motions.scenes()) {
            const lightengine::MotionTarget target = motions.evaluate(scene, 2U, 4U, beat, 0.7, 42U);
            if (target.x < 0.0 || target.x > 1.0 || target.y < 0.0 || target.y > 1.0 ||
                target.dimmer_scale < 0.0 || target.dimmer_scale > 1.0) {
                std::cerr << "Moving-head scene generated an invalid target: " << scene.id << '\n';
                return 1;
            }
        }
    }

    {
        lightengine::SimpleEngine engine{lightengine::SimpleEngineConfig{}};
        const auto now = std::chrono::steady_clock::now();
        const lightengine::DmxFrame stopped_frame = engine.render_frame(now);
        if (engine.output_active(now)) {
            std::cerr << "SimpleEngine enabled ArtNet output before Start\n";
            return 1;
        }
        if (stopped_frame.at(50) != 85 || stopped_frame.at(52) != 179 || stopped_frame.at(55) != 18 || stopped_frame.at(56) != 0 ||
            stopped_frame.at(57) != 0 || stopped_frame.at(58) != 150) {
            std::cerr << "SimpleEngine did not park moving heads with closed shutter while stopped\n";
            return 1;
        }
        engine.apply_control_command(lightengine::SetRunningCommand{true});
        if (!engine.output_active(now)) {
            std::cerr << "SimpleEngine did not enable ArtNet output after Start\n";
            return 1;
        }
        engine.apply_os2l_event(lightengine::Os2lBeatEvent{12, 100.0, 0.8, false}, now);
        const lightengine::DmxFrame frame = engine.render_frame(now);
        if (frame.at(2) == 0 && frame.at(3) == 0 && frame.at(4) == 0) {
            std::cerr << "SimpleEngine did not render LED bar DMX values while running\n";
            return 1;
        }
        if (frame.at(99) == 0 || (frame.at(100) == 0 && frame.at(101) == 0 && frame.at(102) == 0) ||
            frame.at(103) != 0 || frame.at(104) != 0 || frame.at(105) != 0) {
            std::cerr << "SimpleEngine did not render the RGB PAR using only master and RGB\n";
            return 1;
        }
        if (frame.at(50) == 0 || frame.at(52) == 0 || frame.at(56) != 10 || frame.at(57) == 0) {
            std::cerr << "SimpleEngine did not render the enabled moving-head layer\n";
            return 1;
        }
        engine.apply_control_command(lightengine::TriggerCommand{lightengine::LiveTriggerId::next, 0.0});
        if (engine.state_json(now).find(R"("active_effect":"ball")") == std::string::npos) {
            std::cerr << "SimpleEngine Next Look did not select the next enabled RGB scene\n";
            return 1;
        }
        engine.apply_control_command(lightengine::ApplyPresetCommand{"custom"});
        if (engine.state_json(now).find(R"("preset":"custom")") == std::string::npos ||
            engine.state_json(now).find(R"("active_effect":"ball")") == std::string::npos) {
            std::cerr << "SimpleEngine Custom preset unexpectedly replaced the current show settings\n";
            return 1;
        }
        engine.apply_control_command(lightengine::SetFixtureAddressCommand{
            lightengine::PatchFixtureId::led_bars, 0, 100});
        const lightengine::DmxFrame repatched = engine.render_frame(now);
        if ((repatched.at(99) == 0 && repatched.at(100) == 0 && repatched.at(101) == 0) ||
            engine.state_json(now).find(R"("name":"LED Bar 1","start":100)") == std::string::npos) {
            std::cerr << "SimpleEngine did not apply an LED-bar fixture address\n";
            return 1;
        }
        engine.apply_control_command(lightengine::SetFixtureArmedCommand{lightengine::ArmedFixtureId::strobe, true});
        engine.apply_control_command(lightengine::SetHoldTriggerCommand{lightengine::LiveTriggerId::strobe_out, true});
        const lightengine::DmxFrame strobe_frame = engine.render_frame(now);
        if (strobe_frame.at(0) == 0 || strobe_frame.at(1) == 0) {
            std::cerr << "SimpleEngine did not render the armed strobe fixture\n";
            return 1;
        }
        engine.apply_control_command(lightengine::SetHoldTriggerCommand{lightengine::LiveTriggerId::strobe_out, false});
        engine.apply_control_command(lightengine::SetLayerCommand{lightengine::LayerId::motion, true});
        engine.apply_control_command(lightengine::SetGoboControlCommand{
            true, "static", "cloverleaf", false, false, 0.62});
        const lightengine::DmxFrame gobo_frame = engine.render_frame(now);
        if (engine.state_json(now).find(R"("motion":true)") == std::string::npos || gobo_frame.at(55) != 26) {
            std::cerr << "SimpleEngine did not render the selected manual gobo\n";
            return 1;
        }
        engine.apply_control_command(lightengine::SetColorWheelCommand{true, true, "white", 37});
        if (engine.render_frame(now).at(54) != 37 ||
            engine.state_json(now).find(R"("raw_value":37)") == std::string::npos) {
            std::cerr << "SimpleEngine did not render the manual color-wheel raw value\n";
            return 1;
        }
        const std::string state = engine.state_json(now);
        if (state.find(R"("beat_pos":12)") == std::string::npos || state.find(R"("os2l_connected":true)") == std::string::npos ||
            state.find(R"("music_section":")") == std::string::npos || state.find(R"("music_energy":)") == std::string::npos) {
            std::cerr << "SimpleEngine state does not expose beat sync\n";
            return 1;
        }
        engine.apply_control_command(lightengine::SetArtNetCommand{"192.168.137.2", 0, 3, 16});
        const lightengine::ArtNetEndpoint endpoint = engine.artnet_endpoint();
        if (endpoint.host != "192.168.137.2" || endpoint.universe.value() != 0 ||
            engine.state_json(now).find(R"("artnet_host":"192.168.137.2")") == std::string::npos) {
            std::cerr << "SimpleEngine did not apply ArtNet target changes\n";
            return 1;
        }
        const std::uint8_t last_pan = engine.render_frame(now).at(50);
        const std::uint8_t last_tilt = engine.render_frame(now).at(52);
        engine.apply_control_command(lightengine::SetRunningCommand{false});
        const auto stopping = std::chrono::steady_clock::now();
        const lightengine::DmxFrame reset_frame = engine.render_frame(stopping);
        if (!engine.output_active(stopping) || reset_frame.at(50) != last_pan || reset_frame.at(52) != last_tilt ||
            reset_frame.at(56) != 0 || reset_frame.at(57) != 0 || reset_frame.at(59) != 204) {
            std::cerr << "SimpleEngine did not hold a safe reset/calibration frame after Stop\n";
            return 1;
        }
        const auto reset_finished = stopping + std::chrono::seconds{7};
        if (engine.output_active(reset_finished) || engine.render_frame(reset_finished).at(59) != 0) {
            std::cerr << "SimpleEngine did not finish moving-head calibration after its hold interval\n";
            return 1;
        }
    }

    {
        lightengine::SimpleEngine engine{lightengine::SimpleEngineConfig{}};
        const auto now = std::chrono::steady_clock::now();
        engine.apply_control_command(lightengine::SetRunningCommand{true});
        engine.apply_control_command(lightengine::ApplyPresetCommand{"warmup"});
        engine.apply_control_command(lightengine::SetMotionModeCommand{"center_pulse"});
        engine.apply_os2l_event(lightengine::Os2lBeatEvent{1, 112.0, 0.5, false, false}, now);
        const lightengine::DmxFrame pulse_on = engine.render_frame(now);
        engine.apply_control_command(lightengine::SetBeatPulseCommand{lightengine::BeatPulseTarget::motion, false});
        const lightengine::DmxFrame pulse_off = engine.render_frame(now);
        engine.apply_control_command(lightengine::SetBeatPulseCommand{lightengine::BeatPulseTarget::led, true});
        const std::string pulse_state = engine.state_json(now);
        if (pulse_off.at(57) >= pulse_on.at(57) ||
            pulse_state.find(R"("motion_beat_pulse":false)") == std::string::npos ||
            pulse_state.find(R"("led_beat_pulse":true)") == std::string::npos) {
            std::cerr << "Independent LED and moving-head beat pulse controls did not apply\n";
            return 1;
        }

        engine.apply_os2l_event(lightengine::Os2lBeatEvent{0, 112.0, 0.5, false, false}, now);
        const std::uint8_t gobo_at_zero = engine.render_frame(now).at(55);
        engine.apply_os2l_event(lightengine::Os2lBeatEvent{8, 112.0, 0.5, false, false}, now);
        const std::uint8_t gobo_at_eight = engine.render_frame(now).at(55);
        engine.apply_os2l_event(lightengine::Os2lBeatEvent{16, 112.0, 0.5, false, false}, now);
        const std::uint8_t gobo_at_sixteen = engine.render_frame(now).at(55);
        if (gobo_at_zero != gobo_at_eight || gobo_at_sixteen == gobo_at_zero) {
            std::cerr << "Musical gobo mode did not hold calm patterns for sixteen beats\n";
            return 1;
        }

        engine.apply_control_command(lightengine::ApplyPresetCommand{"warmup"});
        engine.apply_os2l_event(lightengine::Os2lBeatEvent{1, 112.0, 0.5, false, false}, now);
        const lightengine::DmxFrame coordinated = engine.render_frame(now);
        const std::string coordinated_state = engine.state_json(now);
        const bool shared_first_color =
            (coordinated_state.find(R"("color_slots":["white")") != std::string::npos && coordinated.at(54) == 8U) ||
            (coordinated_state.find(R"("color_slots":["red")") != std::string::npos && coordinated.at(54) == 24U) ||
            ((coordinated_state.find(R"("color_slots":["cyan")") != std::string::npos ||
                coordinated_state.find(R"("color_slots":["teal")") != std::string::npos) && coordinated.at(54) == 40U) ||
            ((coordinated_state.find(R"("color_slots":["amber")") != std::string::npos ||
                coordinated_state.find(R"("color_slots":["orange")") != std::string::npos) && coordinated.at(54) == 56U) ||
            (coordinated_state.find(R"("color_slots":["blue")") != std::string::npos && coordinated.at(54) == 72U) ||
            ((coordinated_state.find(R"("color_slots":["yellow")") != std::string::npos ||
                coordinated_state.find(R"("color_slots":["acid")") != std::string::npos) && coordinated.at(54) == 88U) ||
            (coordinated_state.find(R"("color_slots":["green")") != std::string::npos && coordinated.at(54) == 104U) ||
            ((coordinated_state.find(R"("color_slots":["uv")") != std::string::npos ||
                coordinated_state.find(R"("color_slots":["magenta")") != std::string::npos ||
                coordinated_state.find(R"("color_slots":["pink")") != std::string::npos) && coordinated.at(54) == 120U);
        if ((coordinated.at(2) == 0U && coordinated.at(3) == 0U && coordinated.at(4) == 0U) ||
            !shared_first_color || coordinated_state.find(R"("layer_state":{"rgb_scene":)") == std::string::npos) {
            std::cerr << "Warm-up LED flood and moving-head palette colors were not coordinated: LED="
                      << static_cast<int>(coordinated.at(2)) << " head=" << static_cast<int>(coordinated.at(54)) << '\n';
            return 1;
        }

        engine.apply_control_command(lightengine::ApplyPresetCommand{"rgb_hard"});
        const std::string rgb_state = engine.state_json(now);
        engine.apply_control_command(lightengine::TriggerCommand{lightengine::LiveTriggerId::next_color, 0.0});
        const std::string next_rgb_state = engine.state_json(now);
        const std::size_t rgb_slots_start = rgb_state.find(R"("color_slots":[)");
        const std::size_t rgb_slots_end = rgb_slots_start == std::string::npos
            ? std::string::npos
            : rgb_state.find(']', rgb_slots_start);
        const std::string rgb_slots = rgb_slots_end == std::string::npos
            ? std::string{}
            : rgb_state.substr(rgb_slots_start, rgb_slots_end - rgb_slots_start + 1U);
        if (rgb_state.find(R"("enabled_color_palettes":["rgb_hard"])") == std::string::npos ||
            rgb_slots.find("red") == std::string::npos || rgb_slots.find("blue") == std::string::npos ||
            rgb_slots.find("green") == std::string::npos || rgb_slots.find("white") != std::string::npos ||
            rgb_state == next_rgb_state) {
            std::cerr << "Pure RGB preset or manual next-color trigger did not keep and rotate its RGB-only pool\n";
            return 1;
        }
    }

    {
        lightengine::SimpleEngine engine{lightengine::SimpleEngineConfig{}};
        const auto now = std::chrono::steady_clock::now();
        engine.apply_control_command(lightengine::SetRunningCommand{true});
        engine.apply_control_command(lightengine::ApplyPresetCommand{"rave"});
        engine.apply_control_command(lightengine::SetMotionModeCommand{"gobo_chase"});
        engine.apply_os2l_event(lightengine::Os2lBeatEvent{0, 140.0, 0.5, false, false}, now);
        const std::uint8_t calm_gobo = engine.render_frame(now).at(55);
        engine.apply_os2l_event(lightengine::Os2lBeatEvent{48, 140.0, 0.5, false, false}, now);
        const std::uint8_t peak_gobo = engine.render_frame(now).at(55);
        engine.apply_os2l_event(lightengine::Os2lBeatEvent{49, 140.0, 0.5, false, false}, now);
        const std::uint8_t next_peak_gobo = engine.render_frame(now).at(55);
        if (calm_gobo >= 64U || peak_gobo < 64U || next_peak_gobo < 64U || next_peak_gobo == peak_gobo ||
            engine.state_json(now).find(R"("music_section":"peak")") == std::string::npos) {
            std::cerr << "Gobo shaking was not restricted to a predicted musical peak: calm="
                      << static_cast<int>(calm_gobo) << " peak=" << static_cast<int>(peak_gobo)
                      << " next=" << static_cast<int>(next_peak_gobo) << '\n';
            return 1;
        }
    }

    {
        lightengine::SimpleEngine engine{lightengine::SimpleEngineConfig{}};
        const auto now = std::chrono::steady_clock::now();
        engine.apply_control_command(lightengine::SetRunningCommand{true});
        engine.apply_control_command(lightengine::ApplyPresetCommand{"hardstyle"});
        engine.apply_os2l_event(lightengine::Os2lBeatEvent{32, 155.0, 0.5, false, false}, now);
        const std::string buildup = engine.state_json(now);
        if (buildup.find(R"("music_section":"buildup")") == std::string::npos ||
            buildup.find(R"("active_effect":"peak_blocks")") != std::string::npos ||
            buildup.find(R"("active_effect":"strobe")") != std::string::npos) {
            std::cerr << "Hardstyle buildup selected an effect reserved for the absolute peak\n";
            return 1;
        }
    }

    {
        lightengine::SimpleEngine engine{lightengine::SimpleEngineConfig{}};
        const auto now = std::chrono::steady_clock::now();
        engine.apply_control_command(lightengine::SetRunningCommand{true});
        engine.apply_control_command(lightengine::ApplyPresetCommand{"techno"});
        engine.apply_control_command(lightengine::SetMotionModeCommand{"techno_left_right"});
        engine.apply_os2l_event(lightengine::Os2lBeatEvent{0, 138.0, 0.5, false, false}, now);
        const lightengine::DmxFrame left_beat = engine.render_frame(now);
        engine.apply_os2l_event(lightengine::Os2lBeatEvent{1, 138.0, 0.5, false, false}, now);
        const lightengine::DmxFrame right_beat = engine.render_frame(now);
        if (left_beat.at(57) == 0U || left_beat.at(68) == 0U || left_beat.at(79) != 0U || left_beat.at(90) != 0U ||
            right_beat.at(57) != 0U || right_beat.at(68) != 0U || right_beat.at(79) == 0U || right_beat.at(90) == 0U) {
            std::cerr << "Techno moving-head scene did not alternate two left/two right heads on full beats\n";
            return 1;
        }

        engine.apply_os2l_event(lightengine::Os2lBeatEvent{48, 138.0, 0.5, false, false}, now);
        const lightengine::DmxFrame automatic_peak = engine.render_frame(now);
        std::size_t automatic_peak_lit = 0;
        for (std::size_t segment = 0; segment < 8U; ++segment) {
            const std::size_t channel = 2U + segment * 3U;
            if (automatic_peak.at(channel) != 0U || automatic_peak.at(channel + 1U) != 0U || automatic_peak.at(channel + 2U) != 0U) {
                ++automatic_peak_lit;
            }
        }
        if (engine.state_json(now).find(R"("active_effect":"peak_blocks")") == std::string::npos || automatic_peak_lit != 4U) {
            std::cerr << "Intense Techno peak did not select the automatic four-segment strobe\n";
            return 1;
        }

        engine.apply_control_command(lightengine::SetHoldTriggerCommand{lightengine::LiveTriggerId::color_strobe, true});
        const lightengine::DmxFrame color_strobe = engine.render_frame(std::chrono::steady_clock::now());
        std::size_t lit_segments = 0;
        for (std::size_t segment = 0; segment < 8U; ++segment) {
            const std::size_t channel = 2U + segment * 3U;
            if (color_strobe.at(channel) != 0U || color_strobe.at(channel + 1U) != 0U || color_strobe.at(channel + 2U) != 0U) {
                ++lit_segments;
            }
        }
        if (lit_segments != 4U) {
            std::cerr << "Color Strobe did not flash one large four-segment block\n";
            return 1;
        }
        engine.apply_control_command(lightengine::SetHoldTriggerCommand{lightengine::LiveTriggerId::color_strobe, false});
    }

    {
        const lightengine::ShowProject loaded = lightengine::load_show_project_from_file("shows/default.json");
        if (loaded.id != "default" || loaded.patch.size() != 10U || loaded.presets.size() != 9U ||
            loaded.presets.at(1).id != "club" || loaded.presets.at(1).effects.size() < 10U ||
            loaded.presets.at(1).motion_scenes.size() < 8U || loaded.presets.at(6).id != "techno") {
            std::cerr << "Show project loader did not load patch and preset scene collections\n";
            return 1;
        }
    }

    {
        std::vector<lightengine::FixtureDefinition> definitions{
            lightengine::FixtureDefinition{
                "rgb-bar",
                "RGB Bar",
                lightengine::FixtureKind::rgb_bar,
                24,
                {
                    {"red", lightengine::DmxCapability{"red", "Red", 1}},
                    {"green", lightengine::DmxCapability{"green", "Green", 2}},
                    {"blue", lightengine::DmxCapability{"blue", "Blue", 3}},
                },
            },
        };
        lightengine::ShowProject project{
            "default",
            "Default Project",
            {lightengine::FixturePatch{"bar-1", "Bar 1", "rgb-bar", lightengine::ArtNetUniverse{0}, lightengine::DmxAddress{1}, true}},
            {lightengine::ShowPreset{"club", "Club", 58, {"pulse"}, {"center_pulse"}}},
            {lightengine::ShowScene{"scene-1", "Scene 1", {"pulse"}, {"center_pulse"}}},
        };

        try {
            lightengine::validate_show_project(project, definitions);
        } catch (const std::exception& error) {
            std::cerr << "Valid project was rejected: " << error.what() << '\n';
            return 1;
        }

        project.patch.at(0).fixture_definition_id = "missing";
        try {
            lightengine::validate_show_project(project, definitions);
            std::cerr << "Invalid project was accepted\n";
            return 1;
        } catch (const std::invalid_argument&) {
        }
    }

    return 0;
}
