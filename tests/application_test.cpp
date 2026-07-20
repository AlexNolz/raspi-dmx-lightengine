#include "lightengine/artnet_sender.hpp"
#include "lightengine/control_command.hpp"
#include "lightengine/engine_input.hpp"
#include "lightengine/os2l_event.hpp"
#include "lightengine/project.hpp"

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
        if (beat.position != 16 || beat.bpm != 116.04 || beat.strength != 0.8 || beat.changed) {
            std::cerr << "OS2L beat event contains wrong values\n";
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
