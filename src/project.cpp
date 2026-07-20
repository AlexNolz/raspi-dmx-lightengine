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

std::unordered_set<std::string> fixture_definition_ids(const std::vector<FixtureDefinition>& definitions) {
    std::unordered_set<std::string> ids;
    for (const FixtureDefinition& definition : definitions) {
        validate_fixture_definition(definition);
        if (!ids.insert(definition.id).second) {
            throw std::invalid_argument{"duplicate fixture definition id: " + definition.id};
        }
    }
    return ids;
}

}  // namespace

void validate_fixture_definition(const FixtureDefinition& definition) {
    require_text(definition.id, "fixture_definition.id");
    require_text(definition.name, "fixture_definition.name");
    if (definition.channel_count == 0 || definition.channel_count > dmx_channel_count) {
        throw std::invalid_argument{"fixture_definition.channel_count must be in range 1..512"};
    }

    std::unordered_set<std::string> capability_ids;
    for (const auto& [key, capability] : definition.capabilities) {
        require_text(key, "fixture_definition.capability_key");
        require_text(capability.id, "fixture_definition.capability.id");
        require_text(capability.name, "fixture_definition.capability.name");
        if (capability.channel == 0 || capability.channel > definition.channel_count) {
            throw std::invalid_argument{"fixture capability channel is outside fixture channel range: " + capability.id};
        }
        if (!capability_ids.insert(capability.id).second) {
            throw std::invalid_argument{"duplicate fixture capability id: " + capability.id};
        }
    }
}

void validate_show_project(const ShowProject& project, const std::vector<FixtureDefinition>& definitions) {
    require_text(project.id, "project.id");
    require_text(project.name, "project.name");

    const std::unordered_set<std::string> definition_ids = fixture_definition_ids(definitions);

    std::unordered_set<std::string> fixture_ids;
    for (const FixturePatch& fixture : project.patch) {
        require_text(fixture.id, "fixture.id");
        require_text(fixture.name, "fixture.name");
        require_text(fixture.fixture_definition_id, "fixture.fixture_definition_id");
        if (!fixture_ids.insert(fixture.id).second) {
            throw std::invalid_argument{"duplicate fixture id: " + fixture.id};
        }
        if (!definition_ids.contains(fixture.fixture_definition_id)) {
            throw std::invalid_argument{"fixture references unknown definition: " + fixture.fixture_definition_id};
        }
    }

    std::unordered_set<std::string> preset_ids;
    for (const ShowPreset& preset : project.presets) {
        require_text(preset.id, "preset.id");
        require_text(preset.name, "preset.name");
        if (!preset_ids.insert(preset.id).second) {
            throw std::invalid_argument{"duplicate preset id: " + preset.id};
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
