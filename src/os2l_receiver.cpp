#include "lightengine/os2l_receiver.hpp"

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <utility>

namespace lightengine {

namespace {

std::runtime_error socket_error(const char* action) {
    return std::runtime_error{std::string{action} + ": " + std::strerror(errno)};
}

}  // namespace

Os2lReceiver::Os2lReceiver(Os2lEndpoint endpoint, MessageHandler handler)
    : endpoint_{std::move(endpoint)}, handler_{std::move(handler)} {
    if (!handler_) {
        throw std::invalid_argument{"OS2L receiver needs a message handler"};
    }
}

Os2lReceiver::~Os2lReceiver() {
    stop();
}

void Os2lReceiver::start() {
    bool expected = false;
    if (!running_.compare_exchange_strong(expected, true)) {
        return;
    }

    socket_ = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_ < 0) {
        running_ = false;
        throw socket_error("socket");
    }

    sockaddr_in local{};
    local.sin_family = AF_INET;
    local.sin_port = htons(endpoint_.port);
    if (::inet_pton(AF_INET, endpoint_.host.c_str(), &local.sin_addr) != 1) {
        ::close(socket_);
        socket_ = -1;
        running_ = false;
        throw std::invalid_argument{"OS2L endpoint host must be an IPv4 address"};
    }

    if (::bind(socket_, reinterpret_cast<const sockaddr*>(&local), sizeof(local)) < 0) {
        ::close(socket_);
        socket_ = -1;
        running_ = false;
        throw socket_error("bind");
    }

    thread_ = std::thread{&Os2lReceiver::receive_loop, this};
}

void Os2lReceiver::stop() {
    if (!running_.exchange(false)) {
        return;
    }

    if (socket_ >= 0) {
        ::shutdown(socket_, SHUT_RDWR);
        ::close(socket_);
        socket_ = -1;
    }

    if (thread_.joinable()) {
        thread_.join();
    }
}

void Os2lReceiver::receive_loop() {
    std::array<char, 8192> buffer{};

    while (running_) {
        sockaddr_in remote{};
        socklen_t remote_size = sizeof(remote);
        const auto received = ::recvfrom(
            socket_,
            buffer.data(),
            buffer.size(),
            0,
            reinterpret_cast<sockaddr*>(&remote),
            &remote_size);

        if (received <= 0) {
            if (!running_) {
                break;
            }
            continue;
        }

        std::array<char, INET_ADDRSTRLEN> remote_address{};
        const char* host = ::inet_ntop(AF_INET, &remote.sin_addr, remote_address.data(), remote_address.size());
        Os2lMessage message{
            std::string{buffer.data(), static_cast<std::size_t>(received)},
            host == nullptr ? std::string{} : std::string{host},
            ntohs(remote.sin_port),
            std::chrono::steady_clock::now(),
        };
        handler_(message);
    }
}

}  // namespace lightengine
