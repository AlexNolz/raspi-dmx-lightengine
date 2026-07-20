#include "lightengine/web_server.hpp"

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <cstring>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace lightengine {

namespace {

constexpr std::size_t max_request_bytes = 64U * 1024U;

std::runtime_error socket_error(const char* action) {
    return std::runtime_error{std::string{action} + ": " + std::strerror(errno)};
}

std::string reason_phrase(const int status) {
    switch (status) {
        case 200:
            return "OK";
        case 400:
            return "Bad Request";
        case 403:
            return "Forbidden";
        case 404:
            return "Not Found";
        case 405:
            return "Method Not Allowed";
        case 413:
            return "Payload Too Large";
        default:
            return "Internal Server Error";
    }
}

std::string content_type_for(const std::filesystem::path& path) {
    const std::string extension = path.extension().string();
    if (extension == ".html") {
        return "text/html; charset=utf-8";
    }
    if (extension == ".css") {
        return "text/css; charset=utf-8";
    }
    if (extension == ".js") {
        return "application/javascript; charset=utf-8";
    }
    if (extension == ".json") {
        return "application/json";
    }
    return "application/octet-stream";
}

std::string header_value(const std::string& request, const std::string& key) {
    const std::string lowered_key = key + ":";
    std::istringstream stream{request};
    std::string line;
    std::getline(stream, line);
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.empty()) {
            return {};
        }
        if (line.size() >= lowered_key.size()) {
            std::string prefix = line.substr(0, lowered_key.size());
            for (char& c : prefix) {
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            }
            if (prefix == lowered_key) {
                std::string value = line.substr(lowered_key.size());
                while (!value.empty() && value.front() == ' ') {
                    value.erase(value.begin());
                }
                return value;
            }
        }
    }
    return {};
}

std::string request_body(const std::string& request) {
    const std::size_t split = request.find("\r\n\r\n");
    if (split == std::string::npos) {
        return {};
    }
    return request.substr(split + 4U);
}

void send_response(const int client_socket, const WebResponse& response) {
    std::ostringstream output;
    output << "HTTP/1.0 " << response.status << ' ' << reason_phrase(response.status) << "\r\n"
           << "Content-Type: " << response.content_type << "\r\n"
           << "Content-Length: " << response.body.size() << "\r\n"
           << "Connection: close\r\n"
           << "Cache-Control: no-store\r\n"
           << "\r\n"
           << response.body;
    const std::string text = output.str();
    ::send(client_socket, text.data(), text.size(), 0);
}

bool path_has_parent_reference(const std::string& path) {
    return path.find("..") != std::string::npos;
}

}  // namespace

WebServer::WebServer(WebEndpoint endpoint, StateHandler state_handler, ControlHandler control_handler)
    : endpoint_{std::move(endpoint)}, state_handler_{std::move(state_handler)}, control_handler_{std::move(control_handler)} {
    if (!state_handler_) {
        throw std::invalid_argument{"Web server needs a state handler"};
    }
    if (!control_handler_) {
        throw std::invalid_argument{"Web server needs a control handler"};
    }
}

WebServer::~WebServer() {
    stop();
}

void WebServer::start() {
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
        throw std::invalid_argument{"Web endpoint host must be an IPv4 address"};
    }

    if (::bind(server_socket_, reinterpret_cast<const sockaddr*>(&local), sizeof(local)) < 0) {
        ::close(server_socket_);
        server_socket_ = -1;
        running_ = false;
        throw socket_error("bind");
    }

    if (::listen(server_socket_, 8) < 0) {
        ::close(server_socket_);
        server_socket_ = -1;
        running_ = false;
        throw socket_error("listen");
    }

    accept_thread_ = std::thread{&WebServer::accept_loop, this};
}

void WebServer::stop() {
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

void WebServer::accept_loop() {
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
            &WebServer::handle_client,
            this,
            client_socket,
            host == nullptr ? std::string{} : std::string{host},
            ntohs(remote.sin_port));
    }
}

void WebServer::handle_client(const int client_socket, const std::string remote_host, const std::uint16_t remote_port) {
    std::string request;
    std::array<char, 4096> buffer{};
    while (request.size() < max_request_bytes) {
        const auto received = ::recv(client_socket, buffer.data(), buffer.size(), 0);
        if (received <= 0) {
            break;
        }
        request.append(buffer.data(), static_cast<std::size_t>(received));

        const std::size_t split = request.find("\r\n\r\n");
        if (split != std::string::npos) {
            const std::string length_text = header_value(request, "content-length");
            const std::size_t content_length = length_text.empty() ? 0U : static_cast<std::size_t>(std::stoul(length_text));
            if (request.size() >= split + 4U + content_length) {
                break;
            }
        }
    }

    const WebResponse response = request.size() >= max_request_bytes
        ? WebResponse{413, "application/json", R"({"ok":false,"error":"request too large"})"}
        : route_request(request, remote_host, remote_port);
    send_response(client_socket, response);
    ::close(client_socket);
}

WebResponse WebServer::route_request(const std::string& request, const std::string& remote_host, const std::uint16_t remote_port) const {
    std::istringstream stream{request};
    std::string method;
    std::string path;
    std::string version;
    stream >> method >> path >> version;

    if (method.empty() || path.empty()) {
        return WebResponse{400, "application/json", R"({"ok":false,"error":"bad request"})"};
    }

    const std::size_t query_start = path.find('?');
    if (query_start != std::string::npos) {
        path = path.substr(0, query_start);
    }

    if (method == "GET" && path == "/api/state") {
        return WebResponse{200, "application/json", state_handler_()};
    }

    if (method == "POST" && path == "/api/control") {
        return control_handler_(WebControlRequest{request_body(request), remote_host, remote_port});
    }

    if (method == "GET") {
        return serve_static_file(path);
    }

    return WebResponse{405, "application/json", R"({"ok":false,"error":"method not allowed"})"};
}

WebResponse WebServer::serve_static_file(const std::string& raw_path) const {
    std::string path = raw_path;
    if (path == "/" || path == "/web" || path == "/web/") {
        path = "/index.html";
    }
    if (path == "/app.css") {
        path = "/app.css";
    }
    if (path == "/app.js") {
        path = "/app.js";
    }
    if (path_has_parent_reference(path)) {
        return WebResponse{403, "text/plain; charset=utf-8", "Forbidden\n"};
    }
    if (!path.empty() && path.front() == '/') {
        path.erase(path.begin());
    }

    const std::filesystem::path file_path = endpoint_.web_root / path;
    if (!std::filesystem::exists(file_path) || !std::filesystem::is_regular_file(file_path)) {
        return WebResponse{404, "text/plain; charset=utf-8", "Not Found\n"};
    }

    std::ifstream file{file_path, std::ios::binary};
    std::ostringstream body;
    body << file.rdbuf();
    return WebResponse{200, content_type_for(file_path), body.str()};
}

}  // namespace lightengine
