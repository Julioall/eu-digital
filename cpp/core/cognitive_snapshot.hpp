#pragma once

#include "digest.hpp"

#include <cstdint>
#include <stdexcept>
#include <string>

namespace eu_digital {

class CognitiveSnapshotError : public std::runtime_error {
public:
    explicit CognitiveSnapshotError(const std::string& message) : std::runtime_error(message) {}
};

struct CognitiveSnapshot {
    std::string timestamp;
    std::string checksum;
    std::string configuration_fingerprint;
    std::string last_applied_event_id;
    std::string payload_json; // Store payload as raw JSON string for simplicity in C++

    static CognitiveSnapshot create(std::string timestamp,
                                    std::string configuration_fingerprint,
                                    std::string last_applied_event_id,
                                    std::string payload_json) {
        CognitiveSnapshot snapshot;
        snapshot.timestamp = std::move(timestamp);
        snapshot.configuration_fingerprint = std::move(configuration_fingerprint);
        snapshot.last_applied_event_id = std::move(last_applied_event_id);
        snapshot.payload_json = std::move(payload_json);
        
        std::string serialized = "{\"configuration_fingerprint\":\"" + escape_json(snapshot.configuration_fingerprint) +
                                 "\",\"last_applied_event_id\":\"" + escape_json(snapshot.last_applied_event_id) +
                                 "\",\"payload\":" + snapshot.payload_json +
                                 ",\"schema_version\":\"1.0\",\"timestamp\":\"" + escape_json(snapshot.timestamp) + "\"}";
                                 
        snapshot.checksum = digest::hex(digest::sha256(serialized));
        return snapshot;
    }

    std::string to_json() const {
        return "{\"checksum\":\"" + escape_json(checksum) +
               "\",\"configuration_fingerprint\":\"" + escape_json(configuration_fingerprint) +
               "\",\"last_applied_event_id\":\"" + escape_json(last_applied_event_id) +
               "\",\"payload\":" + payload_json +
               ",\"schema_version\":\"1.0\",\"timestamp\":\"" + escape_json(timestamp) + "\"}";
    }

private:
    static std::string escape_json(const std::string& value) {
        std::string output;
        output.reserve(value.size());
        for (const char character : value) {
            switch (character) {
            case '"': output += "\\\""; break;
            case '\\': output += "\\\\"; break;
            case '\n': output += "\\n"; break;
            case '\r': output += "\\r"; break;
            case '\t': output += "\\t"; break;
            default:
                if (static_cast<unsigned char>(character) < 0x20) {
                    constexpr char hex[] = "0123456789abcdef";
                    const auto byte = static_cast<unsigned char>(character);
                    output += "\\u00";
                    output.push_back(hex[(byte >> 4) & 0x0f]);
                    output.push_back(hex[byte & 0x0f]);
                } else {
                    output.push_back(character);
                }
                break;
            }
        }
        return output;
    }
};

}  // namespace eu_digital
