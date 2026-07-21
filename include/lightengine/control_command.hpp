#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <variant>

namespace lightengine {

enum class OutputMasterTarget {
    led,
    motion,
};

enum class LayerId {
    led_bars,
    motion,
    strobe,
    fog,
};

enum class ArmedFixtureId {
    strobe,
    fog,
};

enum class LiveTriggerId {
    next,
    whiteout,
    color_strobe,
    strobe_out,
    fog,
    blackout,
};

enum class PatchFixtureId {
    led_bars,
    moving_heads,
    strobe,
    fog,
};

struct SetMoodCommand final {
    std::uint8_t mood{};
    bool mark_custom{true};
};

struct SetMasterCommand final {
    double value{1.0};
};

struct SetOutputMasterCommand final {
    OutputMasterTarget target{OutputMasterTarget::led};
    double value{1.0};
};

struct SetRunningCommand final {
    bool running{true};
};

struct SetBlackoutCommand final {
    bool blackout{false};
};

struct SetLayerCommand final {
    LayerId layer{LayerId::led_bars};
    bool enabled{true};
};

struct SetMotionModeCommand final {
    std::string motion_mode;
};

struct SetFixtureArmedCommand final {
    ArmedFixtureId fixture{ArmedFixtureId::strobe};
    bool armed{false};
};

struct SetStrobeBeatPulseCommand final {
    bool enabled{false};
};

struct SetStrobeMasterCommand final {
    double value{1.0};
};

struct SetStrobeSpeedCommand final {
    double value{1.0};
};

struct ApplyPresetCommand final {
    std::string preset;
};

struct ToggleEffectCommand final {
    std::string effect;
    bool enabled{true};
};

struct ToggleMotionSceneCommand final {
    std::string scene;
    bool enabled{true};
};

struct SetGoboControlCommand final {
    bool enabled{false};
    std::string mode{"beat_step"};
    std::string selected_gobo{"open"};
    bool highpoint_only{false};
    bool shake_enabled{false};
    double shake_mood_threshold{0.62};
};

struct SetColorWheelCommand final {
    bool enabled{false};
    bool use_raw_value{false};
    std::string selected_color{"white"};
    std::uint8_t raw_value{8};
};

struct TriggerCommand final {
    LiveTriggerId trigger{LiveTriggerId::next};
    double seconds{0.0};
};

struct SetHoldTriggerCommand final {
    LiveTriggerId trigger{LiveTriggerId::blackout};
    bool held{false};
};

struct SetArtNetCommand final {
    std::string host;
    std::uint16_t universe{};
    std::uint16_t led_start_channel{1};
    std::uint8_t segment_count{1};
};

struct SetFixtureAddressCommand final {
    PatchFixtureId fixture{PatchFixtureId::led_bars};
    std::uint16_t index{};
    std::uint16_t start{1};
};

struct UnknownControlCommand final {
    std::string action;
    std::string payload;
};

using ControlCommand = std::variant<
    SetMoodCommand,
    SetMasterCommand,
    SetOutputMasterCommand,
    SetRunningCommand,
    SetBlackoutCommand,
    SetLayerCommand,
    SetMotionModeCommand,
    SetFixtureArmedCommand,
    SetStrobeBeatPulseCommand,
    SetStrobeMasterCommand,
    SetStrobeSpeedCommand,
    ApplyPresetCommand,
    ToggleEffectCommand,
    ToggleMotionSceneCommand,
    SetGoboControlCommand,
    SetColorWheelCommand,
    TriggerCommand,
    SetHoldTriggerCommand,
    SetArtNetCommand,
    SetFixtureAddressCommand,
    UnknownControlCommand>;

[[nodiscard]] std::optional<ControlCommand> parse_control_command(const std::string& payload);
[[nodiscard]] std::optional<OutputMasterTarget> parse_output_master_target(const std::string& value);
[[nodiscard]] std::optional<LayerId> parse_layer_id(const std::string& value);
[[nodiscard]] std::optional<ArmedFixtureId> parse_armed_fixture_id(const std::string& value);
[[nodiscard]] std::optional<LiveTriggerId> parse_live_trigger_id(const std::string& value);
[[nodiscard]] std::optional<PatchFixtureId> parse_patch_fixture_id(const std::string& value);
[[nodiscard]] const char* control_command_type_name(const ControlCommand& command) noexcept;

}  // namespace lightengine
