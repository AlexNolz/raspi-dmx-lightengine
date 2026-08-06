#include "lightengine/control_command.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstdlib>
#include <string_view>
#include <unordered_map>

namespace lightengine {

namespace {

using JsonFields = std::unordered_map<std::string, std::string>;

double clamp_double(const double value, const double low, const double high) {
    return std::max(low, std::min(high, value));
}

std::int64_t clamp_i64(const std::int64_t value, const std::int64_t low, const std::int64_t high) {
    return std::max(low, std::min(high, value));
}

void skip_space(std::string_view text, std::size_t& index) {
    while (index < text.size() && std::isspace(static_cast<unsigned char>(text.at(index))) != 0) {
        ++index;
    }
}

std::optional<std::string> parse_json_string(std::string_view text, std::size_t& index) {
    if (index >= text.size() || text.at(index) != '"') {
        return std::nullopt;
    }
    ++index;

    std::string value;
    while (index < text.size()) {
        const char current = text.at(index++);
        if (current == '"') {
            return value;
        }
        if (current == '\\') {
            if (index >= text.size()) {
                return std::nullopt;
            }
            const char escaped = text.at(index++);
            switch (escaped) {
                case '"':
                case '\\':
                case '/':
                    value.push_back(escaped);
                    break;
                case 'b':
                    value.push_back('\b');
                    break;
                case 'f':
                    value.push_back('\f');
                    break;
                case 'n':
                    value.push_back('\n');
                    break;
                case 'r':
                    value.push_back('\r');
                    break;
                case 't':
                    value.push_back('\t');
                    break;
                default:
                    value.push_back(escaped);
                    break;
            }
        } else {
            value.push_back(current);
        }
    }

    return std::nullopt;
}

std::string parse_raw_json_value(std::string_view text, std::size_t& index) {
    const std::size_t start = index;
    while (index < text.size()) {
        const char current = text.at(index);
        if (current == ',' || current == '}') {
            break;
        }
        ++index;
    }

    std::string value{text.substr(start, index - start)};
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), [](unsigned char c) {
        return std::isspace(c) == 0;
    }));
    value.erase(std::find_if(value.rbegin(), value.rend(), [](unsigned char c) {
        return std::isspace(c) == 0;
    }).base(), value.end());
    return value;
}

std::optional<JsonFields> parse_flat_json_object(std::string_view text) {
    std::size_t index = 0;
    skip_space(text, index);
    if (index >= text.size() || text.at(index) != '{') {
        return std::nullopt;
    }
    ++index;

    JsonFields fields;
    while (index < text.size()) {
        skip_space(text, index);
        if (index < text.size() && text.at(index) == '}') {
            return fields;
        }

        std::optional<std::string> key = parse_json_string(text, index);
        if (!key) {
            return std::nullopt;
        }

        skip_space(text, index);
        if (index >= text.size() || text.at(index) != ':') {
            return std::nullopt;
        }
        ++index;
        skip_space(text, index);

        std::string value;
        if (index < text.size() && text.at(index) == '"') {
            std::optional<std::string> parsed_value = parse_json_string(text, index);
            if (!parsed_value) {
                return std::nullopt;
            }
            value = *parsed_value;
        } else {
            value = parse_raw_json_value(text, index);
        }
        fields.emplace(std::move(*key), std::move(value));

        skip_space(text, index);
        if (index < text.size() && text.at(index) == ',') {
            ++index;
            continue;
        }
        if (index < text.size() && text.at(index) == '}') {
            return fields;
        }
        return std::nullopt;
    }

    return std::nullopt;
}

std::optional<std::string> field_string(const JsonFields& fields, const std::string& name) {
    const auto item = fields.find(name);
    if (item == fields.end()) {
        return std::nullopt;
    }
    return item->second;
}

std::optional<double> field_double(const JsonFields& fields, const std::string& name) {
    const auto item = fields.find(name);
    if (item == fields.end()) {
        return std::nullopt;
    }

    char* end = nullptr;
    const double value = std::strtod(item->second.c_str(), &end);
    if (end == item->second.c_str()) {
        return std::nullopt;
    }
    return value;
}

std::optional<std::int64_t> field_i64(const JsonFields& fields, const std::string& name) {
    const auto item = fields.find(name);
    if (item == fields.end()) {
        return std::nullopt;
    }

    std::int64_t value{};
    const auto begin = item->second.data();
    const auto end = begin + item->second.size();
    const auto result = std::from_chars(begin, end, value);
    if (result.ec != std::errc{} || result.ptr != end) {
        return std::nullopt;
    }
    return value;
}

std::optional<bool> field_bool(const JsonFields& fields, const std::string& name) {
    const auto item = fields.find(name);
    if (item == fields.end()) {
        return std::nullopt;
    }

    std::string value = item->second;
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    if (value == "true" || value == "1" || value == "on" || value == "down") {
        return true;
    }
    if (value == "false" || value == "0" || value == "off" || value == "up") {
        return false;
    }
    return std::nullopt;
}

std::uint8_t field_u8_clamped(const JsonFields& fields, const std::string& name, const std::uint8_t fallback, const std::int64_t low, const std::int64_t high) {
    return static_cast<std::uint8_t>(clamp_i64(field_i64(fields, name).value_or(fallback), low, high));
}

std::uint16_t field_u16_clamped(const JsonFields& fields, const std::string& name, const std::uint16_t fallback, const std::int64_t low, const std::int64_t high) {
    return static_cast<std::uint16_t>(clamp_i64(field_i64(fields, name).value_or(fallback), low, high));
}

double field_unit_interval(const JsonFields& fields, const std::string& name, const double fallback) {
    return clamp_double(field_double(fields, name).value_or(fallback), 0.0, 1.0);
}

}  // namespace

std::optional<OutputMasterTarget> parse_output_master_target(const std::string& value) {
    if (value == "led_master") {
        return OutputMasterTarget::led;
    }
    if (value == "motion_master") {
        return OutputMasterTarget::motion;
    }
    if (value == "rgb_par_master") {
        return OutputMasterTarget::rgb_par;
    }
    return std::nullopt;
}

std::optional<BeatPulseTarget> parse_beat_pulse_target(const std::string& value) {
    if (value == "led") {
        return BeatPulseTarget::led;
    }
    if (value == "motion") {
        return BeatPulseTarget::motion;
    }
    return std::nullopt;
}

std::optional<LayerId> parse_layer_id(const std::string& value) {
    if (value == "led_bars") {
        return LayerId::led_bars;
    }
    if (value == "motion") {
        return LayerId::motion;
    }
    if (value == "strobe") {
        return LayerId::strobe;
    }
    if (value == "fog") {
        return LayerId::fog;
    }
    return std::nullopt;
}

std::optional<ArmedFixtureId> parse_armed_fixture_id(const std::string& value) {
    if (value == "strobe") {
        return ArmedFixtureId::strobe;
    }
    if (value == "fog") {
        return ArmedFixtureId::fog;
    }
    return std::nullopt;
}

std::optional<LiveTriggerId> parse_live_trigger_id(const std::string& value) {
    if (value == "next") {
        return LiveTriggerId::next;
    }
    if (value == "next_color") {
        return LiveTriggerId::next_color;
    }
    if (value == "whiteout") {
        return LiveTriggerId::whiteout;
    }
    if (value == "color_strobe") {
        return LiveTriggerId::color_strobe;
    }
    if (value == "strobe_out") {
        return LiveTriggerId::strobe_out;
    }
    if (value == "fog") {
        return LiveTriggerId::fog;
    }
    if (value == "blackout") {
        return LiveTriggerId::blackout;
    }
    return std::nullopt;
}

std::optional<PatchFixtureId> parse_patch_fixture_id(const std::string& value) {
    if (value == "led_bars") {
        return PatchFixtureId::led_bars;
    }
    if (value == "rgb_pars") {
        return PatchFixtureId::rgb_pars;
    }
    if (value == "moving_heads") {
        return PatchFixtureId::moving_heads;
    }
    if (value == "strobe") {
        return PatchFixtureId::strobe;
    }
    if (value == "fog") {
        return PatchFixtureId::fog;
    }
    return std::nullopt;
}

std::optional<ControlCommand> parse_control_command(const std::string& payload) {
    const std::optional<JsonFields> fields = parse_flat_json_object(payload);
    if (!fields) {
        return std::nullopt;
    }

    const std::string action = field_string(*fields, "action").value_or("");
    if (action == "set_mood") {
        return SetMoodCommand{
            field_u8_clamped(*fields, "mood", 58, 0, 100),
            field_bool(*fields, "custom").value_or(true),
        };
    }
    if (action == "set_master") {
        return SetMasterCommand{field_unit_interval(*fields, "master", 1.0)};
    }
    if (action == "set_output_master") {
        const auto target = parse_output_master_target(field_string(*fields, "target").value_or(""));
        if (!target) {
            return UnknownControlCommand{action, payload};
        }
        return SetOutputMasterCommand{*target, field_unit_interval(*fields, "value", 1.0)};
    }
    if (action == "set_running") {
        return SetRunningCommand{field_bool(*fields, "running").value_or(true)};
    }
    if (action == "set_blackout") {
        return SetBlackoutCommand{field_bool(*fields, "blackout").value_or(false)};
    }
    if (action == "set_layer") {
        const auto layer = parse_layer_id(field_string(*fields, "layer").value_or(""));
        if (!layer) {
            return UnknownControlCommand{action, payload};
        }
        return SetLayerCommand{*layer, field_bool(*fields, "enabled").value_or(true)};
    }
    if (action == "set_motion_mode") {
        return SetMotionModeCommand{field_string(*fields, "motion_mode").value_or("auto")};
    }
    if (action == "set_fixture_armed") {
        const auto fixture = parse_armed_fixture_id(field_string(*fields, "fixture").value_or(""));
        if (!fixture) {
            return UnknownControlCommand{action, payload};
        }
        return SetFixtureArmedCommand{*fixture, field_bool(*fields, "armed").value_or(false)};
    }
    if (action == "set_strobe_beat_pulse") {
        return SetStrobeBeatPulseCommand{field_bool(*fields, "enabled").value_or(false)};
    }
    if (action == "set_beat_pulse") {
        const auto target = parse_beat_pulse_target(field_string(*fields, "target").value_or(""));
        if (!target) {
            return UnknownControlCommand{action, payload};
        }
        return SetBeatPulseCommand{*target, field_bool(*fields, "enabled").value_or(true)};
    }
    if (action == "set_strobe_master") {
        return SetStrobeMasterCommand{field_unit_interval(*fields, "value", 1.0)};
    }
    if (action == "set_strobe_speed") {
        return SetStrobeSpeedCommand{field_unit_interval(*fields, "value", 1.0)};
    }
    if (action == "apply_preset") {
        return ApplyPresetCommand{field_string(*fields, "preset").value_or("club")};
    }
    if (action == "toggle_effect") {
        return ToggleEffectCommand{
            field_string(*fields, "effect").value_or(""),
            field_bool(*fields, "enabled").value_or(true),
        };
    }
    if (action == "toggle_motion_scene") {
        return ToggleMotionSceneCommand{
            field_string(*fields, "scene").value_or(""),
            field_bool(*fields, "enabled").value_or(true),
        };
    }
    if (action == "toggle_color_palette") {
        return ToggleColorPaletteCommand{
            field_string(*fields, "palette").value_or(""),
            field_bool(*fields, "enabled").value_or(true),
        };
    }
    if (action == "toggle_gobo_pattern") {
        return ToggleGoboPatternCommand{
            field_string(*fields, "gobo").value_or(""),
            field_bool(*fields, "enabled").value_or(true),
        };
    }
    if (action == "set_gobo_control") {
        return SetGoboControlCommand{
            field_bool(*fields, "enabled").value_or(false),
            field_string(*fields, "mode").value_or("beat_step"),
            field_string(*fields, "selected_gobo").value_or("open"),
            field_bool(*fields, "highpoint_only").value_or(false),
            field_bool(*fields, "shake_enabled").value_or(false),
            clamp_double(field_double(*fields, "shake_mood_threshold").value_or(0.62), 0.0, 1.0),
            field_bool(*fields, "fast_peak_enabled").value_or(true),
        };
    }
    if (action == "set_color_wheel") {
        return SetColorWheelCommand{
            field_bool(*fields, "enabled").value_or(false),
            field_bool(*fields, "use_raw_value").value_or(false),
            field_string(*fields, "selected_color").value_or("white"),
            field_u8_clamped(*fields, "raw_value", 8, 0, 255),
        };
    }
    if (action == "set_rgb_par_zone") {
        return SetRgbParZoneCommand{
            field_bool(*fields, "linked").value_or(false),
            field_u8_clamped(*fields, "mood", 35, 0, 100),
            field_string(*fields, "scene").value_or("auto"),
        };
    }
    if (action == "trigger") {
        const auto trigger = parse_live_trigger_id(field_string(*fields, "name").value_or("next"));
        if (!trigger) {
            return UnknownControlCommand{action, payload};
        }
        return TriggerCommand{*trigger, std::max(0.0, field_double(*fields, "seconds").value_or(0.0))};
    }
    if (action == "set_hold_trigger") {
        const auto trigger = parse_live_trigger_id(field_string(*fields, "name").value_or(""));
        if (!trigger) {
            return UnknownControlCommand{action, payload};
        }
        return SetHoldTriggerCommand{*trigger, field_bool(*fields, "held").value_or(false)};
    }
    if (action == "set_artnet") {
        return SetArtNetCommand{
            field_string(*fields, "artnet_host").value_or("127.0.0.1"),
            field_u16_clamped(*fields, "artnet_universe", 0, 0, 32767),
            field_u16_clamped(*fields, "led_start_channel", 1, 1, 512),
            field_u8_clamped(*fields, "segment_count", 1, 1, 64),
        };
    }
    if (action == "set_fixture_address") {
        const auto fixture = parse_patch_fixture_id(field_string(*fields, "fixture").value_or(""));
        if (!fixture) {
            return UnknownControlCommand{action, payload};
        }
        return SetFixtureAddressCommand{
            *fixture,
            field_u16_clamped(*fields, "index", 0, 0, 255),
            field_u16_clamped(*fields, "start", 1, 1, 512),
        };
    }
    if (action == "set_disco_ball_calibration") {
        return SetDiscoBallCalibrationCommand{
            field_bool(*fields, "test_mode").value_or(false),
            field_u8_clamped(*fields, "selected_head", 0, 0, 3),
            field_bool(*fields, "adjust_all_tilt").value_or(false),
            clamp_double(field_double(*fields, "tilt").value_or(0.68), 0.0, 1.0),
            {
                clamp_double(field_double(*fields, "pan_1").value_or(0.333), 0.0, 1.0),
                clamp_double(field_double(*fields, "pan_2").value_or(0.333), 0.0, 1.0),
                clamp_double(field_double(*fields, "pan_3").value_or(0.333), 0.0, 1.0),
                clamp_double(field_double(*fields, "pan_4").value_or(0.333), 0.0, 1.0),
            },
        };
    }

    return UnknownControlCommand{action, payload};
}

const char* control_command_type_name(const ControlCommand& command) noexcept {
    if (std::holds_alternative<SetMoodCommand>(command)) {
        return "set_mood";
    }
    if (std::holds_alternative<SetMasterCommand>(command)) {
        return "set_master";
    }
    if (std::holds_alternative<SetOutputMasterCommand>(command)) {
        return "set_output_master";
    }
    if (std::holds_alternative<SetRunningCommand>(command)) {
        return "set_running";
    }
    if (std::holds_alternative<SetBlackoutCommand>(command)) {
        return "set_blackout";
    }
    if (std::holds_alternative<SetLayerCommand>(command)) {
        return "set_layer";
    }
    if (std::holds_alternative<SetMotionModeCommand>(command)) {
        return "set_motion_mode";
    }
    if (std::holds_alternative<SetFixtureArmedCommand>(command)) {
        return "set_fixture_armed";
    }
    if (std::holds_alternative<SetStrobeBeatPulseCommand>(command)) {
        return "set_strobe_beat_pulse";
    }
    if (std::holds_alternative<SetBeatPulseCommand>(command)) {
        return "set_beat_pulse";
    }
    if (std::holds_alternative<SetStrobeMasterCommand>(command)) {
        return "set_strobe_master";
    }
    if (std::holds_alternative<SetStrobeSpeedCommand>(command)) {
        return "set_strobe_speed";
    }
    if (std::holds_alternative<ApplyPresetCommand>(command)) {
        return "apply_preset";
    }
    if (std::holds_alternative<ToggleEffectCommand>(command)) {
        return "toggle_effect";
    }
    if (std::holds_alternative<ToggleMotionSceneCommand>(command)) {
        return "toggle_motion_scene";
    }
    if (std::holds_alternative<ToggleColorPaletteCommand>(command)) {
        return "toggle_color_palette";
    }
    if (std::holds_alternative<ToggleGoboPatternCommand>(command)) {
        return "toggle_gobo_pattern";
    }
    if (std::holds_alternative<SetGoboControlCommand>(command)) {
        return "set_gobo_control";
    }
    if (std::holds_alternative<SetColorWheelCommand>(command)) {
        return "set_color_wheel";
    }
    if (std::holds_alternative<SetRgbParZoneCommand>(command)) {
        return "set_rgb_par_zone";
    }
    if (std::holds_alternative<TriggerCommand>(command)) {
        return "trigger";
    }
    if (std::holds_alternative<SetHoldTriggerCommand>(command)) {
        return "set_hold_trigger";
    }
    if (std::holds_alternative<SetArtNetCommand>(command)) {
        return "set_artnet";
    }
    if (std::holds_alternative<SetFixtureAddressCommand>(command)) {
        return "set_fixture_address";
    }
    if (std::holds_alternative<SetDiscoBallCalibrationCommand>(command)) {
        return "set_disco_ball_calibration";
    }
    return "unknown";
}

}  // namespace lightengine
