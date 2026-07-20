#pragma once

#include "lightengine/dmx.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace lightengine {

enum class FixtureKind {
    rgb_bar,
    moving_head,
    dimmer,
    custom,
};

struct FixtureProfile final {
    std::string id;
    std::string name;
    FixtureKind kind{FixtureKind::custom};
    std::uint16_t channel_count{};
};

struct FixturePatch final {
    std::string id;
    std::string name;
    std::string profile_id;
    ArtNetUniverse universe{0};
    DmxAddress address{1};
    bool enabled{true};
};

struct ShowScene final {
    std::string id;
    std::string name;
};

struct LightProject final {
    std::string id;
    std::string name;
    std::vector<FixtureProfile> profiles;
    std::vector<FixturePatch> fixtures;
    std::vector<ShowScene> scenes;
};

void validate_project(const LightProject& project);

}  // namespace lightengine
