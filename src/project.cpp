#include "lightengine/project.hpp"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <regex>
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

std::string string_field(const std::string& object, const std::string& key, const std::string& fallback = {}) {
    const std::regex pattern{'"' + key + R"json("\s*:\s*"([^"]*)")json"};
    std::smatch match;
    return std::regex_search(object, match, pattern) ? match[1].str() : fallback;
}

int integer_field(const std::string& object, const std::string& key, const int fallback) {
    const std::regex pattern{'"' + key + R"json("\s*:\s*([0-9]+))json"};
    std::smatch match;
    return std::regex_search(object, match, pattern) ? std::stoi(match[1].str()) : fallback;
}

bool bool_field(const std::string& object, const std::string& key, const bool fallback) {
    const std::regex pattern{'"' + key + R"json("\s*:\s*(true|false))json"};
    std::smatch match;
    return std::regex_search(object, match, pattern) ? match[1].str() == "true" : fallback;
}

std::string array_for_key(const std::string& text, const std::string& key) {
    const std::size_t key_at = text.find('"' + key + '"');
    const std::size_t open = key_at == std::string::npos ? std::string::npos : text.find('[', key_at);
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
        } else if (current == '"') {
            in_string = true;
        } else if (current == '[') {
            ++depth;
        } else if (current == ']' && --depth == 0) {
            return text.substr(open, index - open + 1U);
        }
    }
    return {};
}

std::vector<std::string> object_items(const std::string& array) {
    std::vector<std::string> objects;
    int depth = 0;
    bool in_string = false;
    bool escaped = false;
    std::size_t start = std::string::npos;
    for (std::size_t index = 0; index < array.size(); ++index) {
        const char current = array.at(index);
        if (in_string) {
            if (escaped) {
                escaped = false;
            } else if (current == '\\') {
                escaped = true;
            } else if (current == '"') {
                in_string = false;
            }
        } else if (current == '"') {
            in_string = true;
        } else if (current == '{') {
            if (depth++ == 0) {
                start = index;
            }
        } else if (current == '}' && --depth == 0 && start != std::string::npos) {
            objects.push_back(array.substr(start, index - start + 1U));
            start = std::string::npos;
        }
    }
    return objects;
}

std::vector<std::string> string_items(const std::string& array) {
    const std::regex pattern{R"json("([^"]+)")json"};
    std::vector<std::string> values;
    for (auto item = std::sregex_iterator{array.begin(), array.end(), pattern}; item != std::sregex_iterator{}; ++item) {
        values.push_back((*item)[1].str());
    }
    return values;
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

ShowProject load_show_project_from_file(const std::string& path) {
    std::ifstream file{path};
    if (!file) {
        throw std::runtime_error{"cannot open show project: " + path};
    }
    const std::string text{std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{}};
    ShowProject project;
    project.id = string_field(text, "id");
    project.name = string_field(text, "name", project.id);
    for (const std::string& object : object_items(array_for_key(text, "patch"))) {
        const int universe = std::clamp(integer_field(object, "universe", 0), 0, 32767);
        const int address = std::clamp(integer_field(object, "address", 1), 1, static_cast<int>(dmx_channel_count));
        project.patch.push_back(FixturePatch{
            string_field(object, "id"),
            string_field(object, "name"),
            string_field(object, "fixture"),
            ArtNetUniverse{static_cast<std::uint16_t>(universe)},
            DmxAddress{static_cast<std::uint16_t>(address)},
            bool_field(object, "enabled", true),
        });
    }
    for (const std::string& object : object_items(array_for_key(text, "presets"))) {
        project.presets.push_back(ShowPreset{
            string_field(object, "id"),
            string_field(object, "name"),
            static_cast<std::uint8_t>(std::clamp(integer_field(object, "mood", 58), 0, 100)),
            string_items(array_for_key(object, "effects")),
            string_items(array_for_key(object, "motion_scenes")),
        });
    }
    for (const std::string& object : object_items(array_for_key(text, "scenes"))) {
        project.scenes.push_back(ShowScene{
            string_field(object, "id"),
            string_field(object, "name"),
            string_items(array_for_key(object, "effects")),
            string_items(array_for_key(object, "motion_scenes")),
        });
    }
    if (project.id.empty() || project.presets.empty()) {
        throw std::runtime_error{"show project has no id or presets: " + path};
    }
    return project;
}

}  // namespace lightengine
