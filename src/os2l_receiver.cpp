#include "lightengine/os2l_receiver.hpp"

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <cctype>
#include <cstring>
#include <stdexcept>
#include <utility>

namespace lightengine {

namespace {

std::runtime_error socket_error(const char* action) {
    return std::runtime_error{std::string{action} + ": " + std::strerror(errno)};
}

std::string trim_left_json_stream(std::string text) {
    const auto first = std::find_if(text.begin(), text.end(), [](unsigned char value) {
        return value != 0xEFU && value != 0xBBU && value != 0xBFU && std::isspace(value) == 0;
    });
    text.erase(text.begin(), first);
    return text;
}

std::size_t find_json_object_end(const std::string& text) {
    bool in_string = false;
    bool escaped = false;
    int depth = 0;

    for (std::size_t index = 0; index < text.size(); ++index) {
        const char value = text.at(index);

        if (escaped) {
            escaped = false;
            continue;
        }

        if (value == '\\' && in_string) {
            escaped = true;
            continue;
        }

        if (value == '"') {
            in_string = !in_string;
            continue;
        }

        if (in_string) {
            continue;
        }

        if (value == '{') {
            ++depth;
        } else if (value == '}') {
            --depth;
            if (depth == 0) {
                return index + 1U;
            }
        }
    }

    return std::string::npos;
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

    server_socket_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket_ < 0) {
        running_ = false;
        throw socket_error("socket");
    }

    int reuse = 1;
    if (::setsockopt(server_socket_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        ::close(server_socket_);
        server_socket_ = -1;
        running_ = false;
        throw socket_error("setsockopt");
    }

    sockaddr_in local{};
    local.sin_family = AF_INET;
    local.sin_port = htons(endpoint_.port);
    if (::inet_pton(AF_INET, endpoint_.host.c_str(), &local.sin_addr) != 1) {
        ::close(server_socket_);
        server_socket_ = -1;
        running_ = false;
        throw std::invalid_argument{"OS2L endpoint host must be an IPv4 address"};
    }

    if (::bind(server_socket_, reinterpret_cast<const sockaddr*>(&local), sizeof(local)) < 0) {
        ::close(server_socket_);
        server_socket_ = -1;
        running_ = false;
        throw socket_error("bind");
    }

    if (::listen(server_socket_, 4) < 0) {
        ::close(server_socket_);
        server_socket_ = -1;
        running_ = false;
        throw socket_error("listen");
    }

    accept_thread_ = std::thread{&Os2lReceiver::accept_loop, this};
}

void Os2lReceiver::stop() {
    if (!running_.exchange(false)) {
        return;
    }

    if (server_socket_ >= 0) {
        ::shutdown(server_socket_, SHUT_RDWR);
        ::close(server_socket_);
        server_socket_ = -1;
    }

    if (accept_thread_.joinable()) {
        accept_thread_.join();
    }

    for (std::thread& client_thread : client_threads_) {
        if (client_thread.joinable()) {
            client_thread.join();
        }
    }
    client_threads_.clear();
}

void Os2lReceiver::accept_loop() {
    while (running_) {
        sockaddr_in remote{};
        socklen_t remote_size = sizeof(remote);
        const int client_socket = ::accept(server_socket_, reinterpret_cast<sockaddr*>(&remote), &remote_size);

        if (client_socket < 0) {
            if (!running_) {
                break;
            }
            continue;
        }

        std::array<char, INET_ADDRSTRLEN> remote_address{};
        const char* host = ::inet_ntop(AF_INET, &remote.sin_addr, remote_address.data(), remote_address.size());
        client_threads_.emplace_back(
            &Os2lReceiver::handle_client,
            this,
            client_socket,
            host == nullptr ? std::string{} : std::string{host},
            ntohs(remote.sin_port));
    }
}

void Os2lReceiver::handle_client(const int client_socket, std::string remote_host, const std::uint16_t remote_port) {
    std::array<char, 8192> buffer{};
    std::string stream_buffer;

    while (running_) {
        const auto received = ::recv(client_socket, buffer.data(), buffer.size(), 0);

        if (received <= 0) {
            break;
        }

        emit_stream_messages(
            stream_buffer,
            std::string{buffer.data(), static_cast<std::size_t>(received)},
            remote_host,
            remote_port);
    }

    ::close(client_socket);
}

void Os2lReceiver::emit_stream_messages(
    std::string& stream_buffer,
    const std::string& chunk,
    const std::string& remote_host,
    const std::uint16_t remote_port) {
    stream_buffer += chunk;

    while (running_) {
        stream_buffer = trim_left_json_stream(std::move(stream_buffer));
        if (stream_buffer.empty()) {
            return;
        }

        if (stream_buffer.front() != '{') {
            const auto next_object = stream_buffer.find('{');
            if (next_object == std::string::npos) {
                stream_buffer.clear();
                return;
            }
            stream_buffer.erase(0, next_object);
        }

        const std::size_t end = find_json_object_end(stream_buffer);
        if (end == std::string::npos) {
            if (stream_buffer.size() > 65536U) {
                stream_buffer.erase(0, stream_buffer.size() - 8192U);
            }
            return;
        }

        Os2lMessage message{
            stream_buffer.substr(0, end),
            remote_host,
            remote_port,
            std::chrono::steady_clock::now(),
        };
        stream_buffer.erase(0, end);
        handler_(message);
    }
}

}  // namespace lightengine
