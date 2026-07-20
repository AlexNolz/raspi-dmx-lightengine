#pragma once

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <thread>
#include <vector>

namespace lightengine {

struct WebEndpoint final {
    std::string host{"127.0.0.1"};
    std::uint16_t port{8088};
    std::filesystem::path web_root{"web"};
};

struct WebControlRequest final {
    std::string payload;
    std::string remote_host;
    std::uint16_t remote_port{};
};

struct WebResponse final {
    int status{200};
    std::string content_type{"application/json"};
    std::string body;
};

class WebServer final {
public:
    using StateHandler = std::function<std::string()>;
    using ControlHandler = std::function<WebResponse(const WebControlRequest&)>;

    WebServer(WebEndpoint endpoint, StateHandler state_handler, ControlHandler control_handler);
    ~WebServer();

    WebServer(const WebServer&) = delete;
    WebServer& operator=(const WebServer&) = delete;

    void start();
    void stop();

    [[nodiscard]] bool running() const noexcept {
        return running_.load();
    }

private:
    void accept_loop();
    void handle_client(int client_socket, std::string remote_host, std::uint16_t remote_port);
    [[nodiscard]] WebResponse route_request(const std::string& request, const std::string& remote_host, std::uint16_t remote_port) const;
    [[nodiscard]] WebResponse serve_static_file(const std::string& raw_path) const;

    WebEndpoint endpoint_;
    StateHandler state_handler_;
    ControlHandler control_handler_;
    std::atomic_bool running_{false};
    int server_socket_{-1};
    std::thread accept_thread_;
    std::vector<std::thread> client_threads_;
};

}  // namespace lightengine
