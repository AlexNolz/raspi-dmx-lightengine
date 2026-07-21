#include "lightengine/fixture_runtime.hpp"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <regex>
#include <stdexcept>
#include <unordered_map>

namespace lightengine {

namespace {

std::string object_for_key(const std::string& text, const std::string& key) {
    const std::string quoted_key = '"' + key + '"';
    const std::size_t key_at = text.find(quoted_key);
    if (key_at == std::string::npos) {
        return {};
    }
    const std::size_t open = text.find('{', key_at + quoted_key.size());
    if (open == std::string::npos) {
        return {};
    }
    int depth = 0;
    bool in_string = false;
    bool escaped = false;
    for (std::size_t index = open; index < text.size(); ++index) {
        const char current = text.at(index);
        if (in_string) {
            if (escaped) {
                escaped = false;
            } else if (current == '\\') {
                escaped = true;
            } else if (current == '"') {
                in_string = false;
            }
            continue;
        }
        if (current == '"') {
            in_string = true;
        } else if (current == '{') {
            ++depth;
        } else if (current == '}' && --depth == 0) {
            return text.substr(open, index - open + 1U);
        }
    }
    return {};
}

std::unordered_map<std::string, std::string> string_map(const std::string& object) {
    const std::regex pair_pattern{R"json("([^"]+)"\s*:\s*"([^"]*)")json"};
    std::unordered_map<std::string, std::string> result;
    for (auto item = std::sregex_iterator{object.begin(), object.end(), pair_pattern}; item != std::sregex_iterator{}; ++item) {
        result.emplace((*item)[1].str(), (*item)[2].str());
    }
    return result;
}

std::vector<FixtureWheelSlot> wheel_slots(const std::string& values_object, const std::string& labels_object) {
    const std::regex pair_pattern{R"json("([^"]+)"\s*:\s*(\d+))json"};
    const auto labels = string_map(labels_object);
    std::vector<FixtureWheelSlot> result;
    for (auto item = std::sregex_iterator{values_object.begin(), values_object.end(), pair_pattern}; item != std::sregex_iterator{}; ++item) {
        const std::string id = (*item)[1].str();
        const int raw_value = std::stoi((*item)[2].str());
        if (raw_value < 0 || raw_value > 255) {
            continue;
        }
        const auto label = labels.find(id);
        result.push_back(FixtureWheelSlot{id, label == labels.end() ? id : label->second, static_cast<std::uint8_t>(raw_value)});
    }
    return result;
}

std::uint8_t integer_field(const std::string& object, const std::string& key, const std::uint8_t fallback) {
    const std::regex field_pattern{'"' + key + R"json("\s*:\s*(\d+))json"};
    std::smatch match;
    if (!std::regex_search(object, match, field_pattern)) {
        return fallback;
    }
    return static_cast<std::uint8_t>(std::min(255, std::stoi(match[1].str())));
}

std::optional<std::uint8_t> wheel_value(const std::vector<FixtureWheelSlot>& slots, const std::string& id) {
    const auto item = std::find_if(slots.begin(), slots.end(), [&](const FixtureWheelSlot& slot) { return slot.id == id; });
    return item == slots.end() ? std::nullopt : std::optional<std::uint8_t>{item->value};
}

}  // namespace

RgbWash::RgbWash(DmxAddress red, DmxAddress green, DmxAddress blue)
    : red_{red}, green_{green}, blue_{blue} {}

void RgbWash::render_to(DmxFrame& frame) const {
    frame.at(red_.zero_based()) = color_.r;
    frame.at(green_.zero_based()) = color_.g;
    frame.at(blue_.zero_based()) = color_.b;
}

RgbWashBar::RgbWashBar(const DmxAddress start_address, const std::uint8_t wash_count) {
    washes_.reserve(wash_count);
    for (std::uint8_t index = 0; index < wash_count; ++index) {
        const auto red = static_cast<std::uint16_t>(start_address.value() + static_cast<std::uint16_t>(index) * 3U);
        washes_.emplace_back(DmxAddress{red}, DmxAddress{static_cast<std::uint16_t>(red + 1U)}, DmxAddress{static_cast<std::uint16_t>(red + 2U)});
    }
}

void RgbWashBar::clear() noexcept {
    set_all(Rgb{});
}

void RgbWashBar::set_all(const Rgb color) noexcept {
    for (RgbWash& wash : washes_) {
        wash.set_color(color);
    }
}

void RgbWashBar::set_wash(const std::size_t index, const Rgb color) {
    if (index >= washes_.size()) {
        throw std::out_of_range{"RGB wash index is outside the bar"};
    }
    washes_.at(index).set_color(color);
}

void RgbWashBar::render_to(DmxFrame& frame) const {
    for (const RgbWash& wash : washes_) {
        wash.render_to(frame);
    }
}

Rgb RgbWashBar::wash_color(const std::size_t index) const {
    if (index >= washes_.size()) {
        throw std::out_of_range{"RGB wash index is outside the bar"};
    }
    return washes_.at(index).color();
}

void Zkymzl11MovingHead::render_to(DmxFrame& frame, const Zkymzl11Look& look) const {
    const std::size_t start = start_address_.zero_based();
    if (start + 10U >= frame.size()) {
        throw std::out_of_range{"ZKYMZL 11CH fixture exceeds the DMX universe"};
    }
    frame.at(start + 0U) = look.pan;
    frame.at(start + 1U) = 0;
    frame.at(start + 2U) = look.tilt;
    frame.at(start + 3U) = 0;
    frame.at(start + 4U) = look.color_wheel;
    frame.at(start + 5U) = look.gobo;
    frame.at(start + 6U) = look.shutter;
    frame.at(start + 7U) = look.dimmer;
    frame.at(start + 8U) = look.movement_speed;
    frame.at(start + 9U) = look.reset;
    frame.at(start + 10U) = 0;
}

Zkymzl11Profile Zkymzl11Profile::load_from_file(const std::string& path) {
    std::ifstream file{path};
    if (!file) {
        throw std::runtime_error{"cannot open moving-head fixture profile: " + path};
    }
    const std::string text{std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{}};
    const std::string color = object_for_key(text, "color");
    const std::string gobo = object_for_key(text, "gobo");
    Zkymzl11Profile profile;
    profile.colors = wheel_slots(object_for_key(color, "colors"), object_for_key(color, "labels"));
    profile.gobos = wheel_slots(object_for_key(gobo, "gobos"), object_for_key(gobo, "labels"));
    const std::string manual_test = object_for_key(color, "manual_test");
    profile.color_test_min = integer_field(manual_test, "min", 0);
    profile.color_test_max = integer_field(manual_test, "max", 127);
    profile.color_test_step = std::max<std::uint8_t>(1, integer_field(manual_test, "step", 1));
    profile.color_test_default = integer_field(manual_test, "default", 3);
    if (profile.colors.empty() || profile.gobos.empty()) {
        throw std::runtime_error{"moving-head fixture profile has no color or gobo wheel slots"};
    }
    return profile;
}

std::optional<std::uint8_t> Zkymzl11Profile::color_value(const std::string& id) const {
    return wheel_value(colors, id);
}

std::optional<std::uint8_t> Zkymzl11Profile::gobo_value(const std::string& id) const {
    return wheel_value(gobos, id);
}

}  // namespace lightengine
