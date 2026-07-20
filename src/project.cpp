#include "lightengine/project.hpp"

#include <stdexcept>
#include <unordered_set>

namespace lightengine {

namespace {

void require_text(const std::string& value, const char* field) {
    if (value.empty()) {
        throw std::invalid_argument{std::string{field} + " must not be empty"};
    }
}

}  // namespace

void validate_project(const LightProject& project) {
    require_text(project.id, "project.id");
    require_text(project.name, "project.name");

    std::unordered_set<std::string> profile_ids;
    for (const FixtureProfile& profile : project.profiles) {
        require_text(profile.id, "profile.id");
        require_text(profile.name, "profile.name");
        if (profile.channel_count == 0 || profile.channel_count > dmx_channel_count) {
            throw std::invalid_argument{"profile.channel_count must be in range 1..512"};
        }
        if (!profile_ids.insert(profile.id).second) {
            throw std::invalid_argument{"duplicate fixture profile id: " + profile.id};
        }
    }

    std::unordered_set<std::string> fixture_ids;
    for (const FixturePatch& fixture : project.fixtures) {
        require_text(fixture.id, "fixture.id");
        require_text(fixture.name, "fixture.name");
        require_text(fixture.profile_id, "fixture.profile_id");
        if (!fixture_ids.insert(fixture.id).second) {
            throw std::invalid_argument{"duplicate fixture id: " + fixture.id};
        }
        if (!profile_ids.contains(fixture.profile_id)) {
            throw std::invalid_argument{"fixture references unknown profile: " + fixture.profile_id};
        }
    }

    std::unordered_set<std::string> scene_ids;
    for (const ShowScene& scene : project.scenes) {
        require_text(scene.id, "scene.id");
        require_text(scene.name, "scene.name");
        if (!scene_ids.insert(scene.id).second) {
            throw std::invalid_argument{"duplicate scene id: " + scene.id};
        }
    }
}

}  // namespace lightengine
