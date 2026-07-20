#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <variant>

namespace lightengine {

struct Os2lBeatEvent final {
    std::int64_t position{};
    double bpm{120.0};
    double strength{1.0};
    bool changed{false};
};

struct Os2lButtonEvent final {
    std::string name;
    bool pressed{false};
};

struct Os2lCommandEvent final {
    std::int64_t id{};
    double parameter{};
};

struct Os2lUnknownEvent final {
    std::string event_name;
    std::string payload;
};

using Os2lEvent = std::variant<Os2lBeatEvent, Os2lButtonEvent, Os2lCommandEvent, Os2lUnknownEvent>;

[[nodiscard]] std::optional<Os2lEvent> parse_os2l_event(const std::string& payload);
[[nodiscard]] std::string normalize_os2l_button_name(const std::string& name);
[[nodiscard]] const char* os2l_event_type_name(const Os2lEvent& event) noexcept;

}  // namespace lightengine
