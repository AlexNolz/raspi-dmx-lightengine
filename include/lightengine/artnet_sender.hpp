#pragma once

#include "lightengine/dmx.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace lightengine {

struct ArtNetEndpoint final {
    std::string host{"127.0.0.1"};
    std::uint16_t port{6454};
    ArtNetUniverse universe{0};
};

class ArtDmxPacket final {
public:
    static constexpr std::size_t header_size = 18;

    ArtDmxPacket(ArtNetUniverse universe, std::uint8_t sequence, const DmxFrame& frame);

    [[nodiscard]] const std::vector<std::uint8_t>& bytes() const noexcept {
        return bytes_;
    }

private:
    std::vector<std::uint8_t> bytes_;
};

class ArtNetSender final {
public:
    explicit ArtNetSender(ArtNetEndpoint endpoint);
    ~ArtNetSender();

    ArtNetSender(const ArtNetSender&) = delete;
    ArtNetSender& operator=(const ArtNetSender&) = delete;

    ArtNetSender(ArtNetSender&&) noexcept;
    ArtNetSender& operator=(ArtNetSender&&) noexcept;

    void send(const DmxFrame& frame);

private:
    ArtNetEndpoint endpoint_;
    int socket_{-1};
    std::array<std::uint8_t, 128> address_{};
    std::uint8_t sequence_{1};
};

}  // namespace lightengine
