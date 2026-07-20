#include "lightengine/artnet_sender.hpp"
#include "lightengine/project.hpp"

#include <iostream>
#include <string>

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
        lightengine::LightProject project{
            "default",
            "Default Project",
            {lightengine::FixtureProfile{"rgb-bar", "RGB Bar", lightengine::FixtureKind::rgb_bar, 24}},
            {lightengine::FixturePatch{"bar-1", "Bar 1", "rgb-bar", lightengine::ArtNetUniverse{0}, lightengine::DmxAddress{1}, true}},
            {lightengine::ShowScene{"scene-1", "Scene 1"}},
        };

        try {
            lightengine::validate_project(project);
        } catch (const std::exception& error) {
            std::cerr << "Valid project was rejected: " << error.what() << '\n';
            return 1;
        }

        project.fixtures.at(0).profile_id = "missing";
        try {
            lightengine::validate_project(project);
            std::cerr << "Invalid project was accepted\n";
            return 1;
        } catch (const std::invalid_argument&) {
        }
    }

    return 0;
}
