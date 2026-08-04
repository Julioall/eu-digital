#pragma once

#include "core/contracts/port_result.hpp"

#include <algorithm>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace eu_digital::contracts {

inline constexpr char kCognitiveStateSchemaVersion[] = "1.0";

inline std::string state_json_string(const std::string& value) {
    std::ostringstream output;
    output << '"';
    for (const unsigned char character : value) {
        switch (character) {
        case '"': output << "\\\""; break;
        case '\\': output << "\\\\"; break;
        case '\b': output << "\\b"; break;
        case '\f': output << "\\f"; break;
        case '\n': output << "\\n"; break;
        case '\r': output << "\\r"; break;
        case '\t': output << "\\t"; break;
        default:
            if (character < 0x20U) {
                constexpr char digits[] = "0123456789abcdef";
                output << "\\u00" << digits[(character >> 4U) & 0x0fU]
                       << digits[character & 0x0fU];
            } else {
                output << static_cast<char>(character);
            }
        }
    }
    output << '"';
    return output.str();
}

inline std::string state_json_array(const std::vector<std::string>& values) {
    std::ostringstream output;
    output << '[';
    for (std::size_t index = 0; index < values.size(); ++index) {
        if (index) output << ',';
        output << state_json_string(values[index]);
    }
    output << ']';
    return output.str();
}

struct CognitiveStateFragmentV1 {
    std::string schema_version{kCognitiveStateSchemaVersion};
    std::string provider_id;
    std::string state_schema_version;
    std::map<std::string, std::string> entries;

    bool valid() const {
        return schema_version == kCognitiveStateSchemaVersion &&
               !provider_id.empty() && !state_schema_version.empty() &&
               !entries.empty() &&
               std::all_of(entries.begin(), entries.end(), [](const auto& item) {
                   return !item.first.empty();
               });
    }

    std::string to_json() const {
        std::ostringstream output;
        output << "{\"entries\":{";
        bool first = true;
        for (const auto& [key, value] : entries) {
            if (!first) output << ',';
            first = false;
            output << state_json_string(key) << ':' << state_json_string(value);
        }
        output << "},\"provider_id\":" << state_json_string(provider_id)
               << ",\"schema_version\":\"1.0\",\"state_schema_version\":"
               << state_json_string(state_schema_version) << '}';
        return output.str();
    }
};

struct CognitiveCoordinatorCheckpointV1 {
    std::string schema_version{kCognitiveStateSchemaVersion};
    std::string policy_id;
    std::vector<std::string> seen_event_ids;

    bool valid() const {
        return schema_version == kCognitiveStateSchemaVersion &&
               !policy_id.empty() &&
               std::all_of(seen_event_ids.begin(), seen_event_ids.end(),
                           [](const auto& value) { return !value.empty(); }) &&
               std::is_sorted(seen_event_ids.begin(), seen_event_ids.end()) &&
               std::adjacent_find(seen_event_ids.begin(), seen_event_ids.end()) ==
                   seen_event_ids.end();
    }

    std::string to_json() const {
        return "{\"policy_id\":" + state_json_string(policy_id) +
               ",\"schema_version\":\"1.0\",\"seen_event_ids\":" +
               state_json_array(seen_event_ids) + '}';
    }
};

struct CognitiveStateBundleV1 {
    std::string schema_version{kCognitiveStateSchemaVersion};
    CognitiveCoordinatorCheckpointV1 coordinator;
    std::vector<std::string> required_provider_ids;
    std::vector<CognitiveStateFragmentV1> fragments;

    bool valid() const {
        if (schema_version != kCognitiveStateSchemaVersion || !coordinator.valid() ||
            !std::is_sorted(required_provider_ids.begin(),
                            required_provider_ids.end()) ||
            std::adjacent_find(required_provider_ids.begin(),
                               required_provider_ids.end()) !=
                required_provider_ids.end() ||
            std::any_of(required_provider_ids.begin(), required_provider_ids.end(),
                        [](const auto& value) { return value.empty(); }) ||
            !std::is_sorted(fragments.begin(), fragments.end(),
                            [](const auto& left, const auto& right) {
                                return left.provider_id < right.provider_id;
                            }) ||
            !std::all_of(fragments.begin(), fragments.end(),
                         [](const auto& fragment) { return fragment.valid(); })) {
            return false;
        }
        std::vector<std::string> fragment_ids;
        fragment_ids.reserve(fragments.size());
        for (const auto& fragment : fragments) fragment_ids.push_back(fragment.provider_id);
        return fragment_ids == required_provider_ids;
    }

    std::string to_json() const {
        std::ostringstream output;
        output << "{\"coordinator\":" << coordinator.to_json()
               << ",\"fragments\":[";
        for (std::size_t index = 0; index < fragments.size(); ++index) {
            if (index) output << ',';
            output << fragments[index].to_json();
        }
        output << "],\"required_provider_ids\":"
               << state_json_array(required_provider_ids)
               << ",\"schema_version\":\"1.0\"}";
        return output.str();
    }
};

struct CognitiveStateRestoreResultV1 {
    std::string schema_version{kCognitiveStateSchemaVersion};
    std::string provider_id;
    std::size_t restored_entries{0};

    bool valid() const {
        return schema_version == kCognitiveStateSchemaVersion &&
               !provider_id.empty() && restored_entries > 0;
    }
};

}  // namespace eu_digital::contracts
