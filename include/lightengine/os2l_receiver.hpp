#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <string>
#include <thread>

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
    void receive_loop();

    Os2lEndpoint endpoint_;
    MessageHandler handler_;
    std::atomic_bool running_{false};
    int socket_{-1};
    std::thread thread_;
};

}  // namespace lightengine
