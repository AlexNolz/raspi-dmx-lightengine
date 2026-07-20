#include "lightengine/artnet_sender.hpp"
#include "lightengine/control_command.hpp"
#include "lightengine/os2l_event.hpp"
#include "lightengine/os2l_receiver.hpp"
#include "lightengine/simple_engine.hpp"
#include "lightengine/version.hpp"
#include "lightengine/web_server.hpp"

#include <atomic>
#include <chrono>
#include <csignal>
#include <exception>
#include <iostream>
#include <optional>
#include <string>
#include <thread>
#include <vector>
#include <variant>

namespace {

std::atomic_bool keep_running{true};

void stop_handler(int) {
    keep_running = false;
}

std::uint16_t parse_u16(const char* text) {
    const int value = std::stoi(text);
    if (value < 0 || value > 65535) {
        throw std::out_of_range{"value must be in range 0..65535"};
    }
    return static_cast<std::uint16_t>(value);
}

std::uint8_t parse_u8(const char* text) {
    const int value = std::stoi(text);
    if (value < 0 || value > 255) {
        throw std::out_of_range{"value must be in range 0..255"};
    }
    return static_cast<std::uint8_t>(value);
}

void print_usage() {
    std::cout
        << "Usage:\n"
        << "  light-engine\n"
        << "  light-engine --artnet-test <ipv4> <universe>\n"
        << "  light-engine --listen-os2l <ipv4> <port>   # TCP OS2L server\n"
        << "  light-engine --serve-web <ipv4> <port> <web-root>\n"
        << "  light-engine --run-simple-engine <web-ip> <web-port> <web-root> <os2l-ip> <os2l-port> <artnet-ip> <universe>\n";
}

void print_os2l_event(const lightengine::Os2lMessage& message) {
    const std::optional<lightengine::Os2lEvent> event = lightengine::parse_os2l_event(message.payload);
    std::cout << "OS2L " << message.remote_host << ':' << message.remote_port << ' ';

    if (!event) {
        std::cout << "invalid " << message.payload << '\n';
        return;
    }

    std::visit(
        [&](const auto& typed_event) {
            using Event = std::decay_t<decltype(typed_event)>;
            if constexpr (std::is_same_v<Event, lightengine::Os2lBeatEvent>) {
                std::cout << "beat pos=" << typed_event.position << " bpm=" << typed_event.bpm
                          << " strength=" << typed_event.strength << " changed="
                          << (typed_event.changed ? "true" : "false") << '\n';
            } else if constexpr (std::is_same_v<Event, lightengine::Os2lButtonEvent>) {
                std::cout << "button name=" << typed_event.name << " pressed="
                          << (typed_event.pressed ? "true" : "false") << '\n';
            } else if constexpr (std::is_same_v<Event, lightengine::Os2lCommandEvent>) {
                std::cout << "command id=" << typed_event.id << " parameter=" << typed_event.parameter << '\n';
            } else {
                std::cout << "unknown evt=" << typed_event.event_name << ' ' << typed_event.payload << '\n';
            }
        },
        *event);
}

std::string demo_state_json() {
    return R"({
  "running": false,
  "blackout": false,
  "active_effect": "pulse",
  "active_effect_label": "Beat Pulse",
  "bpm": 120.0,
  "beat_count": 0,
  "beat_pos": 0,
  "last_os2l_age": null,
  "os2l_connected": false,
  "os2l_connections": 0,
  "artnet_packets": 0,
  "last_error": "",
  "config": {
    "artnet_host": "127.0.0.1",
    "artnet_universe": 0,
    "led_start_channel": 3,
    "segment_count": 16,
    "master": 1.0,
    "led_master": 1.0,
    "motion_master": 1.0,
    "mood": 58,
    "preset": "club",
    "motion_mode": "auto",
    "enabled_effects": ["pulse", "ball", "rainbow", "scanner", "sparkle", "comet", "gate", "fill"],
    "enabled_motion_scenes": ["center_pulse", "point_chase", "line_sweep", "depth_sweep", "cross_pairs"],
    "layers": {"led_bars": true, "motion": true, "strobe": false, "fog": false},
    "fixtures": {
      "led_bars": [
        {"name": "LED Bar 1", "start": 3, "segments": 8, "enabled": true},
        {"name": "LED Bar 2", "start": 27, "segments": 8, "enabled": true}
      ],
      "moving_heads": [
        {"name": "MH 1", "start": 51, "channels": 11, "enabled": true},
        {"name": "MH 2", "start": 62, "channels": 11, "enabled": true},
        {"name": "MH 3", "start": 73, "channels": 11, "enabled": true},
        {"name": "MH 4", "start": 84, "channels": 11, "enabled": true}
      ],
      "strobe": {"name": "Stairville 1500W Strobe", "start": 1, "channels": 2, "enabled": true, "armed": false, "beat_pulse": false, "master": 1.0, "speed": 1.0},
      "fog": {"name": "Stairville AF-40 DMX Fog", "start": 95, "channels": 1, "enabled": true, "armed": false}
    }
  },
  "effects": {
    "pulse": "Beat Pulse",
    "ball": "Game Ball",
    "rainbow": "Rainbow Chase",
    "scanner": "Scanner",
    "sparkle": "Sparkle",
    "comet": "RGB Comet",
    "gate": "Beat Gate",
    "fill": "Fill Chase"
  },
  "motion_modes": {
    "auto": "Auto",
    "center": "Mitte",
    "center_pulse": "Mitte Pulse",
    "point_chase": "Punkt Chase",
    "line_sweep": "Links/Rechts Sweep",
    "depth_sweep": "Vorne/Hinten Sweep",
    "cross_pairs": "2 Links / 2 Rechts"
  },
  "motion_scenes": {
    "center": "Mitte",
    "center_pulse": "Mitte Pulse",
    "point_chase": "Punkt Chase",
    "line_sweep": "Links/Rechts Sweep",
    "depth_sweep": "Vorne/Hinten Sweep",
    "cross_pairs": "2 Links / 2 Rechts"
  },
  "presets": ["lounge", "club", "rave", "game_show", "rgb_hard", "custom"],
  "show": {},
  "preview": [
    {"r":255,"g":0,"b":0},{"r":255,"g":80,"b":0},{"r":255,"g":180,"b":0},{"r":0,"g":255,"b":40},
    {"r":0,"g":210,"b":255},{"r":0,"g":70,"b":255},{"r":120,"g":0,"b":255},{"r":255,"g":0,"b":220},
    {"r":255,"g":0,"b":0},{"r":255,"g":80,"b":0},{"r":255,"g":180,"b":0},{"r":0,"g":255,"b":40},
    {"r":0,"g":210,"b":255},{"r":0,"g":70,"b":255},{"r":120,"g":0,"b":255},{"r":255,"g":0,"b":220}
  ]
})";
}

}  // namespace

int main(int argc, char** argv) {
    try {
        if (argc > 1) {
            const std::string command{argv[1]};
            if (command == "--help" || command == "-h") {
                print_usage();
                return 0;
            }

            if (command == "--artnet-test") {
                if (argc != 4) {
                    print_usage();
                    return 2;
                }
                lightengine::DmxFrame frame{};
                frame.at(0) = 255;
                lightengine::ArtNetSender sender{lightengine::ArtNetEndpoint{
                    argv[2],
                    6454,
                    lightengine::ArtNetUniverse{parse_u16(argv[3])},
                }};
                sender.send(frame);
                std::cout << "Sent ArtDMX test packet to " << argv[2] << " universe " << argv[3] << '\n';
                return 0;
            }

            if (command == "--listen-os2l") {
                if (argc != 4) {
                    print_usage();
                    return 2;
                }
                std::signal(SIGINT, stop_handler);
                std::signal(SIGTERM, stop_handler);

                lightengine::Os2lReceiver receiver{
                    lightengine::Os2lEndpoint{argv[2], parse_u16(argv[3])},
                    [](const lightengine::Os2lMessage& message) { print_os2l_event(message); },
                };
                receiver.start();
                std::cout << "Listening for OS2L TCP on " << argv[2] << ':' << argv[3] << '\n';
                while (keep_running) {
                    std::this_thread::sleep_for(std::chrono::milliseconds{100});
                }
                receiver.stop();
                return 0;
            }

            if (command == "--serve-web") {
                if (argc != 5) {
                    print_usage();
                    return 2;
                }
                std::signal(SIGINT, stop_handler);
                std::signal(SIGTERM, stop_handler);

                lightengine::WebServer server{
                    lightengine::WebEndpoint{argv[2], parse_u16(argv[3]), argv[4]},
                    [] { return demo_state_json(); },
                    [](const lightengine::WebControlRequest& request) {
                        const std::optional<lightengine::ControlCommand> command =
                            lightengine::parse_control_command(request.payload);
                        if (!command) {
                            return lightengine::WebResponse{400, "application/json", R"({"ok":false,"error":"invalid control payload"})"};
                        }
                        std::cout << "WEB " << request.remote_host << ':' << request.remote_port << ' '
                                  << lightengine::control_command_type_name(*command) << ' ' << request.payload << '\n';
                        return lightengine::WebResponse{200, "application/json", demo_state_json()};
                    },
                };
                server.start();
                std::cout << "C++ web UI: http://" << argv[2] << ':' << argv[3] << '\n';
                while (keep_running) {
                    std::this_thread::sleep_for(std::chrono::milliseconds{100});
                }
                server.stop();
                return 0;
            }

            if (command == "--run-simple-engine") {
                if (argc != 9) {
                    print_usage();
                    return 2;
                }
                std::signal(SIGINT, stop_handler);
                std::signal(SIGTERM, stop_handler);

                lightengine::SimpleEngine engine{lightengine::SimpleEngineConfig{
                    argv[7],
                    parse_u16(argv[8]),
                    3,
                    27,
                    8,
                }};
                lightengine::ArtNetSender artnet{lightengine::ArtNetEndpoint{
                    argv[7],
                    6454,
                    lightengine::ArtNetUniverse{parse_u16(argv[8])},
                }};

                lightengine::Os2lReceiver os2l{
                    lightengine::Os2lEndpoint{argv[5], parse_u16(argv[6])},
                    [&](const lightengine::Os2lMessage& message) {
                        const std::optional<lightengine::Os2lEvent> event = lightengine::parse_os2l_event(message.payload);
                        if (event) {
                            engine.apply_os2l_event(*event, message.received_at);
                        }
                    },
                };

                lightengine::WebServer web{
                    lightengine::WebEndpoint{argv[2], parse_u16(argv[3]), argv[4]},
                    [&] { return engine.state_json(std::chrono::steady_clock::now()); },
                    [&](const lightengine::WebControlRequest& request) {
                        const std::optional<lightengine::ControlCommand> control =
                            lightengine::parse_control_command(request.payload);
                        if (!control) {
                            return lightengine::WebResponse{400, "application/json", R"({"ok":false,"error":"invalid control payload"})"};
                        }
                        engine.apply_control_command(*control);
                        return lightengine::WebResponse{200, "application/json", engine.state_json(std::chrono::steady_clock::now())};
                    },
                };

                os2l.start();
                web.start();
                std::thread output_thread{[&] {
                    using clock = std::chrono::steady_clock;
                    while (keep_running) {
                        const auto started = clock::now();
                        artnet.send(engine.render_frame(started));
                        engine.mark_artnet_packet_sent();
                        std::this_thread::sleep_until(started + std::chrono::milliseconds{25});
                    }
                }};

                std::cout << "Simple C++ Light Engine running\n";
                std::cout << "Web:   http://" << argv[2] << ':' << argv[3] << '\n';
                std::cout << "OS2L:  " << argv[5] << ':' << argv[6] << '\n';
                std::cout << "ArtNet " << argv[7] << " universe " << argv[8] << '\n';
                while (keep_running) {
                    std::this_thread::sleep_for(std::chrono::milliseconds{100});
                }
                web.stop();
                os2l.stop();
                if (output_thread.joinable()) {
                    output_thread.join();
                }
                return 0;
            }

            print_usage();
            return 2;
        }

        std::cout << "Raspberry Pi DMX Light Engine " << lightengine::version << '\n';
        std::cout << "C++ main is ready.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Fatal error: " << error.what() << '\n';
        return 1;
    } catch (...) {
        std::cerr << "Fatal error: unknown exception\n";
        return 1;
    }
}
