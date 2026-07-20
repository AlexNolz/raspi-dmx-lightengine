#include "lightengine/artnet_sender.hpp"
#include "lightengine/os2l_event.hpp"
#include "lightengine/os2l_receiver.hpp"
#include "lightengine/version.hpp"

#include <atomic>
#include <chrono>
#include <csignal>
#include <exception>
#include <iostream>
#include <optional>
#include <string>
#include <thread>
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

void print_usage() {
    std::cout
        << "Usage:\n"
        << "  light-engine\n"
        << "  light-engine --artnet-test <ipv4> <universe>\n"
        << "  light-engine --listen-os2l <ipv4> <port>   # TCP OS2L server\n";
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
