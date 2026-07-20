#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <string>
#include <thread>
#include <vector>

namespace lightengine {

struct Os2lEndpoint final {
    std::string host{"0.0.0.0"};
    std::uint16_t port{9996};
};

struct Os2lMessage final {
    std::string payload;
    std::string remote_host;
    std::uint16_t remote_port{};
    std::chrono::steady_clock::time_point received_at{};
};

class Os2lReceiver final {
public:
    using MessageHandler = std::function<void(const Os2lMessage&)>;

    Os2lReceiver(Os2lEndpoint endpoint, MessageHandler handler);
    ~Os2lReceiver();

    Os2lReceiver(const Os2lReceiver&) = delete;
    Os2lReceiver& operator=(const Os2lReceiver&) = delete;

    void start();
    void stop();

    [[nodiscard]] bool running() const noexcept {
        return running_.load();
    }

private:
    void accept_loop();
    void handle_client(int client_socket, std::string remote_host, std::uint16_t remote_port);
    void emit_stream_messages(std::string& stream_buffer, const std::string& chunk, const std::string& remote_host, std::uint16_t remote_port);

    Os2lEndpoint endpoint_;
    MessageHandler handler_;
    std::atomic_bool running_{false};
    int server_socket_{-1};
    std::thread accept_thread_;
    std::vector<std::thread> client_threads_;
};

}  // namespace lightengine
