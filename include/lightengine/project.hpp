#pragma once

#include "lightengine/dmx.hpp"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace lightengine {

enum class FixtureKind {
    rgb_bar,
    moving_head,
    strobe,
    fog,
    dimmer,
    custom,
};

struct DmxCapability final {
    std::string id;
    std::string name;
    std::uint16_t channel{};
};

struct FixtureDefinition final {
    std::string id;
    std::string name;
    FixtureKind kind{FixtureKind::custom};
    std::uint16_t channel_count{};
    std::map<std::string, DmxCapability> capabilities;
};

struct FixturePatch final {
    std::string id;
    std::string name;
    std::string fixture_definition_id;
    ArtNetUniverse universe{0};
    DmxAddress address{1};
    bool enabled{true};
};

struct ShowPreset final {
    std::string id;
    std::string name;
    std::uint8_t mood{58};
    std::vector<std::string> effects;
    std::vector<std::string> motion_scenes;
};

struct ShowScene final {
    std::string id;
    std::string name;
    std::vector<std::string> effects;
    std::vector<std::string> motion_scenes;
};

struct ShowProject final {
    std::string id;
    std::string name;
    std::vector<FixturePatch> patch;
    std::vector<ShowPreset> presets;
    std::vector<ShowScene> scenes;
};

void validate_fixture_definition(const FixtureDefinition& definition);
void validate_show_project(const ShowProject& project, const std::vector<FixtureDefinition>& definitions);
[[nodiscard]] ShowProject load_show_project_from_file(const std::string& path);

}  // namespace lightengine
