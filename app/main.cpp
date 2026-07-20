#include "lightengine/application.hpp"
#include "lightengine/artnet_sender.hpp"
#include "lightengine/os2l_receiver.hpp"

#include <atomic>
#include <chrono>
#include <csignal>
#include <exception>
#include <iostream>
#include <string>
#include <thread>

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
        << "  light-engine --listen-os2l <ipv4> <port>\n";
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
                    [](const lightengine::Os2lMessage& message) {
                        std::cout << "OS2L " << message.remote_host << ':' << message.remote_port << ' '
                                  << message.payload << '\n';
                    },
                };
                receiver.start();
                std::cout << "Listening for OS2L on " << argv[2] << ':' << argv[3] << '\n';
                while (keep_running) {
                    std::this_thread::sleep_for(std::chrono::milliseconds{100});
                }
                receiver.stop();
                return 0;
            }

            print_usage();
            return 2;
        }

        return lightengine::Application{std::cout}.run();
    } catch (const std::exception& error) {
        std::cerr << "Fatal error: " << error.what() << '\n';
        return 1;
    } catch (...) {
        std::cerr << "Fatal error: unknown exception\n";
        return 1;
    }
}
