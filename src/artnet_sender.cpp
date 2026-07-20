#include "lightengine/artnet_sender.hpp"

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>

namespace lightengine {

namespace {

constexpr std::uint16_t artdmx_opcode = 0x5000;
constexpr std::uint16_t artnet_protocol_version = 14;

void put_le16(std::vector<std::uint8_t>& bytes, const std::size_t offset, const std::uint16_t value) {
    bytes.at(offset) = static_cast<std::uint8_t>(value & 0xFFU);
    bytes.at(offset + 1U) = static_cast<std::uint8_t>((value >> 8U) & 0xFFU);
}

void put_be16(std::vector<std::uint8_t>& bytes, const std::size_t offset, const std::uint16_t value) {
    bytes.at(offset) = static_cast<std::uint8_t>((value >> 8U) & 0xFFU);
    bytes.at(offset + 1U) = static_cast<std::uint8_t>(value & 0xFFU);
}

std::runtime_error socket_error(const std::string& action) {
    return std::runtime_error{action + ": " + std::strerror(errno)};
}

}  // namespace

ArtDmxPacket::ArtDmxPacket(const ArtNetUniverse universe, const std::uint8_t sequence, const DmxFrame& frame)
    : bytes_(header_size + frame.size()) {
    const std::array<std::uint8_t, 8> id{'A', 'r', 't', '-', 'N', 'e', 't', '\0'};
    std::copy(id.begin(), id.end(), bytes_.begin());

    put_le16(bytes_, 8, artdmx_opcode);
    put_be16(bytes_, 10, artnet_protocol_version);
    bytes_.at(12) = sequence;
    bytes_.at(13) = 0;
    put_le16(bytes_, 14, universe.value());
    put_be16(bytes_, 16, static_cast<std::uint16_t>(frame.size()));
    std::copy(frame.begin(), frame.end(), bytes_.begin() + static_cast<std::ptrdiff_t>(header_size));
}

ArtNetSender::ArtNetSender(ArtNetEndpoint endpoint) : endpoint_{std::move(endpoint)} {
    socket_ = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_ < 0) {
        throw socket_error("socket");
    }

    sockaddr_in destination{};
    destination.sin_family = AF_INET;
    destination.sin_port = htons(endpoint_.port);
    if (::inet_pton(AF_INET, endpoint_.host.c_str(), &destination.sin_addr) != 1) {
        ::close(socket_);
        socket_ = -1;
        throw std::invalid_argument{"Art-Net endpoint host must be an IPv4 address"};
    }

    static_assert(sizeof(destination) <= std::tuple_size_v<decltype(address_)>);
    std::memcpy(address_.data(), &destination, sizeof(destination));
}

ArtNetSender::~ArtNetSender() {
    if (socket_ >= 0) {
        ::close(socket_);
    }
}

ArtNetSender::ArtNetSender(ArtNetSender&& other) noexcept
    : endpoint_{std::move(other.endpoint_)},
      socket_{other.socket_},
      address_{other.address_},
      sequence_{other.sequence_} {
    other.socket_ = -1;
}

ArtNetSender& ArtNetSender::operator=(ArtNetSender&& other) noexcept {
    if (this != &other) {
        if (socket_ >= 0) {
            ::close(socket_);
        }
        endpoint_ = std::move(other.endpoint_);
        socket_ = other.socket_;
        address_ = other.address_;
        sequence_ = other.sequence_;
        other.socket_ = -1;
    }
    return *this;
}

void ArtNetSender::send(const DmxFrame& frame) {
    const ArtDmxPacket packet{endpoint_.universe, sequence_, frame};
    ++sequence_;
    if (sequence_ == 0) {
        sequence_ = 1;
    }

    sockaddr_in destination{};
    std::memcpy(&destination, address_.data(), sizeof(destination));
    const auto sent = ::sendto(
        socket_,
        packet.bytes().data(),
        packet.bytes().size(),
        0,
        reinterpret_cast<const sockaddr*>(&destination),
        sizeof(destination));

    if (sent < 0 || static_cast<std::size_t>(sent) != packet.bytes().size()) {
        throw socket_error("sendto");
    }
}

}  // namespace lightengine
