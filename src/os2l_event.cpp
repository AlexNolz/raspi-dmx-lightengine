#include "lightengine/os2l_event.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstdlib>
#include <string_view>
#include <unordered_map>

namespace lightengine {

namespace {

using JsonFields = std::unordered_map<std::string, std::string>;

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

std::string lowercase(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
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
    const std::string value = lowercase(item->second);
    if (value == "true" || value == "1" || value == "on" || value == "down") {
        return true;
    }
    if (value == "false" || value == "0" || value == "off" || value == "up") {
        return false;
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

}  // namespace

std::string normalize_os2l_button_name(const std::string& name) {
    std::string normalized;
    for (const unsigned char c : name) {
        if (std::isalnum(c) != 0) {
            normalized.push_back(static_cast<char>(std::tolower(c)));
        }
    }
    return normalized;
}

std::optional<Os2lEvent> parse_os2l_event(const std::string& payload) {
    const std::optional<JsonFields> fields = parse_flat_json_object(payload);
    if (!fields) {
        return std::nullopt;
    }

    const std::string event_name = lowercase(field_string(*fields, "evt").value_or(""));
    if (event_name == "beat") {
        Os2lBeatEvent beat{};
        beat.position = field_i64(*fields, "pos").value_or(0);
        beat.bpm = field_double(*fields, "bpm").value_or(120.0);
        beat.strength = field_double(*fields, "strength").value_or(1.0);
        beat.changed = field_bool(*fields, "change").value_or(false);
        return beat;
    }

    if (event_name == "btn") {
        Os2lButtonEvent button{};
        button.name = normalize_os2l_button_name(field_string(*fields, "name").value_or(""));
        button.pressed = field_bool(*fields, "state").value_or(true);
        return button;
    }

    if (event_name == "cmd") {
        Os2lCommandEvent command{};
        command.id = field_i64(*fields, "id").value_or(0);
        command.parameter = field_double(*fields, "param").value_or(0.0);
        return command;
    }

    return Os2lUnknownEvent{event_name, payload};
}

const char* os2l_event_type_name(const Os2lEvent& event) noexcept {
    if (std::holds_alternative<Os2lBeatEvent>(event)) {
        return "beat";
    }
    if (std::holds_alternative<Os2lButtonEvent>(event)) {
        return "button";
    }
    if (std::holds_alternative<Os2lCommandEvent>(event)) {
        return "command";
    }
    return "unknown";
}

}  // namespace lightengine
