#pragma once

#include "lightengine/control_command.hpp"
#include "lightengine/os2l_event.hpp"

#include <chrono>
#include <string>
#include <variant>

namespace lightengine {

struct EngineBeatInput final {
    Os2lBeatEvent beat;
};

struct EngineButtonInput final {
    Os2lButtonEvent button;
};

struct EngineCommandInput final {
    Os2lCommandEvent command;
};

struct EngineWebControlInput final {
    ControlCommand command;
};

struct EngineUnknownInput final {
    std::string source;
    std::string payload;
};

using EngineInputPayload = std::variant<
    EngineBeatInput,
    EngineButtonInput,
    EngineCommandInput,
    EngineWebControlInput,
    EngineUnknownInput>;

struct EngineInput final {
    EngineInputPayload payload;
    std::chrono::steady_clock::time_point received_at{};
};

[[nodiscard]] inline EngineInput make_engine_input(const Os2lBeatEvent& beat, const std::chrono::steady_clock::time_point received_at) {
    return EngineInput{EngineBeatInput{beat}, received_at};
}

[[nodiscard]] inline EngineInput make_engine_input(const Os2lButtonEvent& button, const std::chrono::steady_clock::time_point received_at) {
    return EngineInput{EngineButtonInput{button}, received_at};
}

[[nodiscard]] inline EngineInput make_engine_input(const Os2lCommandEvent& command, const std::chrono::steady_clock::time_point received_at) {
    return EngineInput{EngineCommandInput{command}, received_at};
}

[[nodiscard]] inline EngineInput make_engine_input(const ControlCommand& command, const std::chrono::steady_clock::time_point received_at) {
    return EngineInput{EngineWebControlInput{command}, received_at};
}

}  // namespace lightengine
