#pragma once

#include "digest.hpp"
#include "core/contracts/cognitive_state_v1.hpp"

#include <cmath>
#include <cstdint>
#include <iomanip>
#include <limits>
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

struct CognitiveSnapshotV2 {
    std::string schema_version{"2.0"};
    std::string captured_at;
    double captured_epoch_seconds{0.0};
    std::string checksum;
    std::string configuration_fingerprint;
    std::string last_applied_event_id;
    contracts::CognitiveStateBundleV1 state;

    static CognitiveSnapshotV2 create(
        std::string captured_at,
        double captured_epoch_seconds,
        std::string configuration_fingerprint,
        std::string last_applied_event_id,
        contracts::CognitiveStateBundleV1 state) {
        CognitiveSnapshotV2 snapshot;
        snapshot.captured_at = std::move(captured_at);
        snapshot.captured_epoch_seconds = captured_epoch_seconds;
        snapshot.configuration_fingerprint =
            std::move(configuration_fingerprint);
        snapshot.last_applied_event_id = std::move(last_applied_event_id);
        snapshot.state = std::move(state);
        if (!snapshot.valid_without_checksum()) {
            throw CognitiveSnapshotError("invalid cognitive snapshot v2 input");
        }
        snapshot.checksum = digest::hex(
            digest::sha256(snapshot.unsigned_json()));
        return snapshot;
    }

    bool valid() const {
        return valid_without_checksum() && checksum.size() == 64 &&
               std::all_of(checksum.begin(), checksum.end(), [](char character) {
                   return (character >= '0' && character <= '9') ||
                          (character >= 'a' && character <= 'f');
               });
    }

    std::string unsigned_json() const {
        std::ostringstream output;
        output << "{\"captured_at\":" << contracts::state_json_string(captured_at)
               << ",\"captured_epoch_seconds\":"
               << std::setprecision(std::numeric_limits<double>::max_digits10)
               << captured_epoch_seconds
               << ",\"configuration_fingerprint\":"
               << contracts::state_json_string(configuration_fingerprint)
               << ",\"last_applied_event_id\":"
               << contracts::state_json_string(last_applied_event_id)
               << ",\"schema_version\":\"2.0\",\"state\":"
               << state.to_json() << '}';
        return output.str();
    }

    std::string to_json() const {
        if (!valid()) throw CognitiveSnapshotError("invalid cognitive snapshot v2");
        auto output = unsigned_json();
        output.pop_back();
        output += ",\"checksum\":" + contracts::state_json_string(checksum) + '}';
        return output;
    }

    static bool serialized_checksum_valid(const std::string& serialized) {
        constexpr std::string_view marker = ",\"checksum\":\"";
        const auto position = serialized.rfind(marker);
        if (position == std::string::npos || serialized.size() !=
                position + marker.size() + 64 + 2) {
            return false;
        }
        const auto checksum = serialized.substr(position + marker.size(), 64);
        if (serialized[serialized.size() - 2] != '"' ||
            serialized.back() != '}') {
            return false;
        }
        const auto unsigned_serialized = serialized.substr(0, position) + '}';
        return digest::hex(digest::sha256(unsigned_serialized)) == checksum;
    }

private:
    bool valid_without_checksum() const {
        return schema_version == "2.0" && !captured_at.empty() &&
               std::isfinite(captured_epoch_seconds) &&
               !configuration_fingerprint.empty() &&
               !last_applied_event_id.empty() && state.valid();
    }
};

}  // namespace eu_digital
