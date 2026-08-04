#pragma once

#include "async_cognitive_snapshot_writer.hpp"
#include "capability_runtime.hpp"
#include "cognitive_coordinator.hpp"
#include "cognitive_snapshot.hpp"
#include "cognitive_state_manager.hpp"
#include "event_bus.hpp"
#include "privacy_storage.hpp"
#include "timeline_store.hpp"

#include <cctype>
#include <chrono>
#include <cstdint>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace eu_digital {

class RuntimeHostError : public std::runtime_error {
public:
    explicit RuntimeHostError(const std::string& message) : std::runtime_error(message) {}
};

enum class RuntimeState { starting, ready, degraded, stopping, stopped, failed };

inline std::string runtime_state_name(RuntimeState state) {
    switch (state) {
    case RuntimeState::starting: return "starting";
    case RuntimeState::ready: return "ready";
    case RuntimeState::degraded: return "degraded";
    case RuntimeState::stopping: return "stopping";
    case RuntimeState::stopped: return "stopped";
    case RuntimeState::failed: return "failed";
    }
    throw RuntimeHostError("unknown runtime state");
}

struct RuntimeConfig {
    std::string manifest_path;
    std::string timeline_path;
    std::string session_id;
    std::string observed_at;
    bool enable_cognitive_coordinator{true};
    bool enable_cognitive_snapshots{false};
    std::size_t cognitive_snapshot_interval_events{100};
    std::chrono::seconds cognitive_snapshot_max_age{
        std::chrono::hours(24)};
    std::chrono::milliseconds cognitive_snapshot_capture_budget{
        std::chrono::milliseconds(5)};
    std::size_t cognitive_snapshot_max_plaintext_bytes{4U * 1024U * 1024U};
};

struct RuntimeManifest {
    std::string runtime_id;
    std::string runtime_version;
    std::string platform;
    std::string compiler;
    std::string profile;
    std::string commit;
    std::map<std::string, std::string> contract_versions;
    std::vector<std::string> optional_capabilities;
};

namespace runtime_detail {

struct JsonNumber {
    std::string text;
};

struct JsonValue {
    using Array = std::vector<JsonValue>;
    using Object = std::map<std::string, JsonValue>;
    std::variant<std::nullptr_t, bool, JsonNumber, std::string, Array, Object> value;
};

class JsonParser {
public:
    explicit JsonParser(std::string_view text) : text_(text) {}

    JsonValue parse() {
        auto result = parse_value();
        skip_space();
        if (position_ != text_.size()) fail("unexpected trailing JSON input");
        return result;
    }

private:
    JsonValue parse_value() {
        skip_space();
        if (position_ == text_.size()) fail("unexpected end of JSON input");
        switch (text_[position_]) {
        case '{': return parse_object();
        case '[': return parse_array();
        case '"': return JsonValue{parse_string()};
        case 't': consume_literal("true"); return JsonValue{true};
        case 'f': consume_literal("false"); return JsonValue{false};
        case 'n': consume_literal("null"); return JsonValue{nullptr};
        default: return JsonValue{JsonNumber{parse_number()}};
        }
    }

    JsonValue parse_object() {
        expect('{');
        JsonValue::Object object;
        skip_space();
        if (consume('}')) return JsonValue{std::move(object)};
        while (true) {
            skip_space();
            if (position_ == text_.size() || text_[position_] != '"') fail("object key must be a string");
            auto key = parse_string();
            if (object.contains(key)) fail("duplicate JSON object key");
            skip_space();
            expect(':');
            object.emplace(std::move(key), parse_value());
            skip_space();
            if (consume('}')) return JsonValue{std::move(object)};
            expect(',');
        }
    }

    JsonValue parse_array() {
        expect('[');
        JsonValue::Array array;
        skip_space();
        if (consume(']')) return JsonValue{std::move(array)};
        while (true) {
            array.push_back(parse_value());
            skip_space();
            if (consume(']')) return JsonValue{std::move(array)};
            expect(',');
        }
    }

    std::string parse_string() {
        expect('"');
        std::string result;
        while (position_ < text_.size()) {
            const char character = text_[position_++];
            if (character == '"') return result;
            if (static_cast<unsigned char>(character) < 0x20) fail("control character in JSON string");
            if (character != '\\') {
                result.push_back(character);
                continue;
            }
            if (position_ == text_.size()) fail("unterminated JSON escape");
            const char escaped = text_[position_++];
            switch (escaped) {
            case '"': result.push_back('"'); break;
            case '\\': result.push_back('\\'); break;
            case '/': result.push_back('/'); break;
            case 'b': result.push_back('\b'); break;
            case 'f': result.push_back('\f'); break;
            case 'n': result.push_back('\n'); break;
            case 'r': result.push_back('\r'); break;
            case 't': result.push_back('\t'); break;
            case 'u': append_utf8(result, parse_hex_codepoint()); break;
            default: fail("unsupported JSON escape");
            }
        }
        fail("unterminated JSON string");
    }

    std::string parse_number() {
        const auto begin = position_;
        if (consume('-')) {}
        if (position_ == text_.size()) fail("invalid JSON number");
        if (text_[position_] == '0') {
            ++position_;
        } else {
            if (!std::isdigit(static_cast<unsigned char>(text_[position_]))) fail("invalid JSON number");
            while (position_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[position_]))) ++position_;
        }
        if (consume('.')) {
            if (position_ == text_.size() || !std::isdigit(static_cast<unsigned char>(text_[position_]))) fail("invalid JSON fraction");
            while (position_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[position_]))) ++position_;
        }
        if (position_ < text_.size() && (text_[position_] == 'e' || text_[position_] == 'E')) {
            ++position_;
            if (position_ < text_.size() && (text_[position_] == '+' || text_[position_] == '-')) ++position_;
            if (position_ == text_.size() || !std::isdigit(static_cast<unsigned char>(text_[position_]))) fail("invalid JSON exponent");
            while (position_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[position_]))) ++position_;
        }
        return std::string(text_.substr(begin, position_ - begin));
    }

    void consume_literal(std::string_view literal) {
        if (text_.substr(position_, literal.size()) != literal) fail("invalid JSON literal");
        position_ += literal.size();
    }

    std::uint32_t parse_hex_codepoint() {
        if (position_ + 4 > text_.size()) fail("short unicode escape");
        std::uint32_t codepoint = 0;
        for (int index = 0; index < 4; ++index) {
            const char character = text_[position_++];
            codepoint <<= 4;
            if (character >= '0' && character <= '9') codepoint |= static_cast<std::uint32_t>(character - '0');
            else if (character >= 'a' && character <= 'f') codepoint |= static_cast<std::uint32_t>(character - 'a' + 10);
            else if (character >= 'A' && character <= 'F') codepoint |= static_cast<std::uint32_t>(character - 'A' + 10);
            else fail("invalid unicode escape");
        }
        return codepoint;
    }

    static void append_utf8(std::string& output, std::uint32_t codepoint) {
        if (codepoint <= 0x7f) output.push_back(static_cast<char>(codepoint));
        else if (codepoint <= 0x7ff) {
            output.push_back(static_cast<char>(0xc0 | (codepoint >> 6)));
            output.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
        } else {
            output.push_back(static_cast<char>(0xe0 | (codepoint >> 12)));
            output.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)));
            output.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
        }
    }

    void skip_space() {
        while (position_ < text_.size() && std::isspace(static_cast<unsigned char>(text_[position_]))) ++position_;
    }

    bool consume(char expected) {
        if (position_ < text_.size() && text_[position_] == expected) {
            ++position_;
            return true;
        }
        return false;
    }

    void expect(char expected) {
        skip_space();
        if (!consume(expected)) fail(std::string("expected JSON character: ") + expected);
    }

    [[noreturn]] static void fail(const std::string& message) { throw RuntimeHostError(message); }

    std::string_view text_;
    std::size_t position_{0};
};

inline const JsonValue::Object& object(const JsonValue& value, const std::string& path) {
    if (!std::holds_alternative<JsonValue::Object>(value.value)) throw RuntimeHostError(path + " must be an object");
    return std::get<JsonValue::Object>(value.value);
}

inline const JsonValue::Array& array(const JsonValue& value, const std::string& path) {
    if (!std::holds_alternative<JsonValue::Array>(value.value)) throw RuntimeHostError(path + " must be an array");
    return std::get<JsonValue::Array>(value.value);
}

inline const JsonValue& required(const JsonValue::Object& object_value, const std::string& key, const std::string& path) {
    const auto found = object_value.find(key);
    if (found == object_value.end()) throw RuntimeHostError(path + " missing required field: " + key);
    return found->second;
}

inline std::string string(const JsonValue& value, const std::string& path) {
    if (!std::holds_alternative<std::string>(value.value)) throw RuntimeHostError(path + " must be a string");
    const auto& result = std::get<std::string>(value.value);
    if (result.empty()) throw RuntimeHostError(path + " must not be empty");
    return result;
}

inline bool boolean(const JsonValue& value, const std::string& path) {
    if (!std::holds_alternative<bool>(value.value)) throw RuntimeHostError(path + " must be a boolean");
    return std::get<bool>(value.value);
}

inline bool is_null(const JsonValue& value) { return std::holds_alternative<std::nullptr_t>(value.value); }

inline std::uint64_t unsigned_number(const JsonValue& value, const std::string& path) {
    if (!std::holds_alternative<JsonNumber>(value.value)) throw RuntimeHostError(path + " must be a non-negative integer");
    const auto& text = std::get<JsonNumber>(value.value).text;
    if (text.empty() || text[0] == '-' || text.find_first_not_of("0123456789") != std::string::npos) {
        throw RuntimeHostError(path + " must be a non-negative integer");
    }
    std::uint64_t result = 0;
    for (const char character : text) {
        const auto digit = static_cast<std::uint64_t>(character - '0');
        if (result > (std::numeric_limits<std::uint64_t>::max() - digit) / 10) {
            throw RuntimeHostError(path + " is too large");
        }
        result = result * 10 + digit;
    }
    return result;
}

inline void exact_keys(const JsonValue::Object& object_value, std::initializer_list<const char*> keys,
                       const std::string& path) {
    if (object_value.size() != keys.size()) throw RuntimeHostError(path + " has unknown or missing fields");
    for (const auto* key : keys) required(object_value, key, path);
}

inline bool semver(const std::string& value) {
    int parts = 0;
    std::size_t start = 0;
    while (start < value.size()) {
        const auto end = value.find('.', start);
        const auto length = (end == std::string::npos ? value.size() : end) - start;
        if (length == 0 || value.substr(start, length).find_first_not_of("0123456789") != std::string::npos) return false;
        ++parts;
        if (end == std::string::npos) break;
        start = end + 1;
    }
    return parts == 3;
}

inline double finite_number(const JsonValue& value, const std::string& path) {
    if (!std::holds_alternative<JsonNumber>(value.value)) {
        throw RuntimeHostError(path + " must be a finite number");
    }
    const auto& text = std::get<JsonNumber>(value.value).text;
    std::size_t parsed = 0;
    double result = 0.0;
    try {
        result = std::stod(text, &parsed);
    } catch (const std::exception&) {
        throw RuntimeHostError(path + " must be a finite number");
    }
    if (parsed != text.size() || !std::isfinite(result)) {
        throw RuntimeHostError(path + " must be a finite number");
    }
    return result;
}

inline std::string string_allow_empty(const JsonValue& value,
                                      const std::string& path) {
    if (!std::holds_alternative<std::string>(value.value)) {
        throw RuntimeHostError(path + " must be a string");
    }
    return std::get<std::string>(value.value);
}

inline std::int64_t days_from_civil(int year, unsigned month, unsigned day) {
    year -= month <= 2;
    const int era = (year >= 0 ? year : year - 399) / 400;
    const unsigned year_of_era = static_cast<unsigned>(year - era * 400);
    const unsigned day_of_year =
        (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
    const unsigned day_of_era = year_of_era * 365 + year_of_era / 4 -
                                 year_of_era / 100 + day_of_year;
    return static_cast<std::int64_t>(era) * 146097 +
           static_cast<std::int64_t>(day_of_era) - 719468;
}

inline double parse_iso8601_epoch(const std::string& value) {
    if (value.size() < 20 || value[4] != '-' || value[7] != '-' ||
        value[10] != 'T' || value[13] != ':' || value[16] != ':') {
        throw RuntimeHostError("timestamp must be a valid ISO-8601 string");
    }
    const auto number = [&](std::size_t offset, std::size_t length) {
        const auto token = value.substr(offset, length);
        if (token.size() != length || token.find_first_not_of("0123456789") !=
                                          std::string::npos) {
            throw RuntimeHostError("timestamp must be a valid ISO-8601 string");
        }
        return std::stoi(token);
    };
    const int year = number(0, 4);
    const unsigned month = static_cast<unsigned>(number(5, 2));
    const unsigned day = static_cast<unsigned>(number(8, 2));
    const int hour = number(11, 2);
    const int minute = number(14, 2);
    const int second = number(17, 2);
    if (month < 1 || month > 12 || day < 1 || day > 31 || hour > 23 ||
        minute > 59 || second > 60) {
        throw RuntimeHostError("timestamp components are outside valid bounds");
    }
    const auto zone_start = value.find_first_of("Z+-", 19);
    if (zone_start == std::string::npos) {
        throw RuntimeHostError("timestamp must include timezone");
    }
    double fraction = 0.0;
    if (zone_start > 19) {
        if (value[19] != '.') {
            throw RuntimeHostError("timestamp fraction is invalid");
        }
        try {
            fraction = std::stod("0" + value.substr(19, zone_start - 19));
        } catch (const std::exception&) {
            throw RuntimeHostError("timestamp fraction is invalid");
        }
    }
    int offset_seconds = 0;
    if (value[zone_start] == 'Z') {
        if (zone_start + 1 != value.size()) {
            throw RuntimeHostError("timestamp timezone is invalid");
        }
    } else {
        if (value.size() != zone_start + 6 || value[zone_start + 3] != ':') {
            throw RuntimeHostError("timestamp timezone is invalid");
        }
        const int offset_hours = number(zone_start + 1, 2);
        const int offset_minutes = number(zone_start + 4, 2);
        if (offset_hours > 23 || offset_minutes > 59) {
            throw RuntimeHostError("timestamp timezone is invalid");
        }
        const int sign = value[zone_start] == '-' ? -1 : 1;
        offset_seconds = sign * (offset_hours * 3600 + offset_minutes * 60);
    }
    return static_cast<double>(
               days_from_civil(year, month, day) * 86400LL + hour * 3600 +
               minute * 60 + second - offset_seconds) +
           fraction;
}

inline std::string escape_json(const std::string& value) {
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

inline std::string read_file(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw RuntimeHostError("cannot open file: " + path);
    std::ostringstream contents;
    contents << input.rdbuf();
    return contents.str();
}

}  // namespace runtime_detail

class RuntimeHost {
public:
    explicit RuntimeHost(RuntimeConfig config) : config_(std::move(config)) {
        if (config_.manifest_path.empty() || config_.timeline_path.empty() || config_.session_id.empty()) {
            throw RuntimeHostError("manifest_path, timeline_path, and session_id are required");
        }
        if (config_.observed_at.empty()) config_.observed_at = "1970-01-01T00:00:00Z";
        if (config_.cognitive_snapshot_interval_events == 0 ||
            config_.cognitive_snapshot_max_age <= std::chrono::seconds::zero() ||
            config_.cognitive_snapshot_capture_budget <=
                std::chrono::milliseconds::zero() ||
            config_.cognitive_snapshot_max_plaintext_bytes == 0) {
            throw RuntimeHostError("cognitive snapshot limits must be positive");
        }
    }

    ~RuntimeHost() { stop(); }

    RuntimeHost(const RuntimeHost&) = delete;
    RuntimeHost& operator=(const RuntimeHost&) = delete;

    bool start() {
        std::lock_guard lock(mutex_);
        if (state_ == RuntimeState::ready || state_ == RuntimeState::degraded) return true;
        if (state_ == RuntimeState::starting || state_ == RuntimeState::stopping) return false;

        state_ = RuntimeState::starting;
        errors_.clear();
        published_events_ = 0;
        recovered_events_ = 0;
        live_committed_events_ = 0;
        recovery_source_ = "not_started";
        recovery_rejections_.clear();
        {
            std::lock_guard snapshot_lock(snapshot_mutex_);
            last_committed_input_.reset();
        }
        try {
            manifest_ = parse_manifest(runtime_detail::read_file(config_.manifest_path));
            timeline_ = std::make_shared<TimelineStore>(config_.timeline_path);
            refresh_storage_usage_locked();
            std::set<std::string> active_implementations;
            for (const auto& [implementation_id, record] :
                 capability_registry_.records()) {
                (void)record;
                active_implementations.insert(implementation_id);
            }
            capability_registry_.define_profile(
                "runtime-host-minimal", std::move(active_implementations));
            capability_registry_.activate_profile("runtime-host-minimal");
            if (config_.enable_cognitive_coordinator) {
                CognitiveCoordinatorConfig coordinator_config;
                coordinator_config.auto_start = false;
                coordinator_ = std::make_shared<CognitiveCoordinator>(
                    capability_registry_, coordinator_config);
                state_manager_ = std::make_unique<CognitiveStateManager>(
                    capability_registry_, *coordinator_,
                    config_.cognitive_snapshot_capture_budget);
                configuration_fingerprint_ =
                    cognitive_configuration_fingerprint(
                        manifest_->runtime_id, manifest_->runtime_version,
                        capability_registry_);
                recover_cognitive_state();
            } else {
                recovered_events_ = count_replayable(timeline_->replay());
                recovery_source_ = "timeline_only";
            }
            const bool snapshot_unavailable =
                config_.enable_cognitive_snapshots && coordinator_ &&
                !LocalDataProtection::available();
            if (config_.enable_cognitive_snapshots && coordinator_ &&
                LocalDataProtection::available()) {
                snapshot_writer_ =
                    std::make_unique<AsyncCognitiveSnapshotWriter>(
                        config_.timeline_path, [this](const std::string& message) {
                            std::lock_guard error_lock(mutex_);
                            add_error_locked("cognitive_snapshot_write_failed",
                                             message, false);
                            if (state_ == RuntimeState::ready) {
                                state_ = RuntimeState::degraded;
                            }
                        },
                        config_.cognitive_snapshot_max_plaintext_bytes);
            } else if (snapshot_unavailable) {
                add_error_locked("cognitive_snapshot_encryption_unavailable",
                                 "local snapshot encryption is unavailable",
                                 false);
            }
            event_bus_ = std::make_shared<EventBus>();
            if (coordinator_) {
                coordinator_->set_publisher([this](const CanonicalEvent& ev) {
                    if (event_bus_) {
                        try { event_bus_->publish(ev); } catch (...) {}
                    }
                });
                coordinator_->set_cycle_commit_handler(
                    [this](const auto& input, const auto&) {
                        on_cycle_committed(input);
                    });
            }
            event_bus_->subscribe({}, {}, [this](const CanonicalEvent& event) {
                persist(event);
                if (coordinator_ && event.event_type != "cognitive.cycle.result") {
                    coordinator_->enqueue_input(make_cycle_input(event, false));
                }
            });

            if (storage_quota_.health().status == StorageStatus::degraded ||
                snapshot_unavailable) {
                state_ = RuntimeState::degraded;
                if (storage_quota_.health().status == StorageStatus::degraded) {
                    add_error_locked("storage_quota_exceeded", "local storage quota requires user decision", false);
                }
            } else if (manifest_->optional_capabilities.empty()) {
                state_ = RuntimeState::ready;
            } else {
                state_ = RuntimeState::degraded;
                for (const auto& capability : manifest_->optional_capabilities) {
                    add_error_locked("optional_capability_unavailable", "optional capability is not installed: " + capability, false);
                }
            }
            return true;
        } catch (const std::exception& error) {
            event_bus_.reset();
            snapshot_writer_.reset();
            state_manager_.reset();
            coordinator_.reset();
            timeline_.reset();
            add_error_locked("runtime_start_failed", error.what(), true);
            state_ = RuntimeState::failed;
            return false;
        }
    }

    void stop() {
        std::shared_ptr<EventBus> event_bus;
        std::shared_ptr<CognitiveCoordinator> coordinator;
        AsyncCognitiveSnapshotWriter* snapshot_writer = nullptr;
        {
            std::lock_guard lock(mutex_);
            if (state_ == RuntimeState::stopped) return;
            event_bus = event_bus_;
            coordinator = coordinator_;
            snapshot_writer = snapshot_writer_.get();
            if (event_bus) state_ = RuntimeState::stopping;
        }
        if (event_bus) event_bus->wait_idle();
        if (coordinator) coordinator->wait_idle();
        if (snapshot_writer) {
            (void)checkpoint_now();
            snapshot_writer->wait_idle();
        }
        if (coordinator) coordinator->stop();
        if (snapshot_writer) snapshot_writer->stop();
        {
            std::lock_guard lock(mutex_);
            event_bus_.reset();
            snapshot_writer_.reset();
            state_manager_.reset();
            coordinator_.reset();
            timeline_.reset();
            if (state_ != RuntimeState::failed) state_ = RuntimeState::stopped;
        }
    }

    PublishResult publish(const CanonicalEvent& event) {
        std::shared_ptr<EventBus> event_bus;
        {
            std::lock_guard lock(mutex_);
            if (state_ != RuntimeState::ready && state_ != RuntimeState::degraded) {
                throw RuntimeHostError("runtime is not accepting events");
            }
            event_bus = event_bus_;
        }
        if (!event_bus) throw RuntimeHostError("event bus is unavailable");
        const auto result = event_bus->publish(event);
        event_bus->wait_idle();
        return result;
    }

    PublishResult publish_json(const std::string& canonical_event_json) {
        return publish(parse_canonical_event(canonical_event_json));
    }

    std::vector<CanonicalEvent> replay() const {
        std::lock_guard lock(mutex_);
        if (!timeline_) throw RuntimeHostError("timeline is unavailable");
        return timeline_->replay();
    }

    RuntimeState state() const {
        std::lock_guard lock(mutex_);
        return state_;
    }
    CapabilityRegistry& capability_registry() { return capability_registry_; }
    const CapabilityRegistry& capability_registry() const { return capability_registry_; }

    EventBus& event_bus() {
        if (!event_bus_) throw RuntimeHostError("event bus is unavailable");
        return *event_bus_;
    }

    std::shared_ptr<CognitiveCoordinator> coordinator() const {
        std::lock_guard lock(mutex_);
        return coordinator_;
    }

    bool checkpoint_now() {
        contracts::CognitiveCycleInputV1 input;
        {
            std::lock_guard snapshot_lock(snapshot_mutex_);
            if (!last_committed_input_) return false;
            input = *last_committed_input_;
        }
        return submit_snapshot(input);
    }

    void wait_snapshot_idle() {
        AsyncCognitiveSnapshotWriter* writer = nullptr;
        {
            std::lock_guard lock(mutex_);
            writer = snapshot_writer_.get();
        }
        if (writer) writer->wait_idle();
    }

    StorageHealth storage_health() const {
        std::lock_guard lock(mutex_);
        return storage_quota_.health();
    }

    std::string storage_health_json() const {
        std::lock_guard lock(mutex_);
        const auto& health = storage_quota_.health();
        std::ostringstream output;
        output << "{\"schema_version\":\"1.0\",\"status\":\""
               << (health.status == StorageStatus::ready ? "ready" : "degraded")
               << "\",\"quota_bytes\":" << health.quota_bytes
               << ",\"user_bytes\":" << health.user_bytes
               << ",\"model_bytes\":" << health.model_bytes
               << ",\"capture_suspended\":" << (health.capture_suspended ? "true" : "false")
               << ",\"user_decision_required\":" << (health.user_decision_required ? "true" : "false")
               << ",\"reason_code\":\"" << runtime_detail::escape_json(health.reason_code) << "\"}";
        const auto result = output.str();
        validate_storage_health_json(result);
        return result;
    }

    std::string cognitive_recovery_json() const {
        std::lock_guard lock(mutex_);
        std::ostringstream output;
        output << "{\"schema_version\":\"1.0\",\"source\":\""
               << runtime_detail::escape_json(recovery_source_)
               << "\",\"replayed_events\":" << recovered_events_
               << ",\"snapshot_rejections\":[";
        for (std::size_t index = 0; index < recovery_rejections_.size();
             ++index) {
            if (index) output << ',';
            output << '"'
                   << runtime_detail::escape_json(recovery_rejections_[index])
                   << '"';
        }
        output << "]}";
        return output.str();
    }

    std::string health_json() const {
        std::lock_guard lock(mutex_);
        const auto event_bus_state = event_bus_ ? (state_ == RuntimeState::failed ? "failed" : "ready") :
            (state_ == RuntimeState::stopped ? "stopped" : "failed");
        const auto timeline_state = timeline_ ? (state_ == RuntimeState::degraded ? "degraded" : "ready") :
            (state_ == RuntimeState::stopped ? "stopped" : "failed");
        std::ostringstream output;
        output << "{\"schema_version\":\"1.0\",\"runtime_id\":\""
               << runtime_detail::escape_json(manifest_ ? manifest_->runtime_id : "unavailable")
               << "\",\"session_id\":\"" << runtime_detail::escape_json(config_.session_id)
               << "\",\"observed_at\":\"" << runtime_detail::escape_json(config_.observed_at)
               << "\",\"state\":\"" << runtime_state_name(state_)
               << "\",\"event_bus\":{\"state\":\"" << event_bus_state
               << "\",\"published_events\":" << published_events_
               << ",\"dead_letters\":" << (event_bus_ ? event_bus_->dead_letters().size() : 0)
               << "},\"timeline\":{\"state\":\"" << timeline_state << "\",\"path\":";
        if (timeline_) output << "\"" << runtime_detail::escape_json(config_.timeline_path) << "\"";
        else output << "null";
        output << ",\"recovered_events\":" << recovered_events_ << "},\"capabilities\":[";
        bool first = true;
        if (manifest_) {
            for (const auto& capability : manifest_->optional_capabilities) {
                if (!first) output << ',';
                first = false;
                output << "{\"capability_id\":\"" << runtime_detail::escape_json(capability)
                       << "\",\"state\":\"temporarily_unavailable\",\"required\":false,\"reason_code\":\"not_installed\"}";
            }
        }
        output << "],\"errors\":[";
        for (std::size_t index = 0; index < errors_.size(); ++index) {
            if (index != 0) output << ',';
            const auto& error = errors_[index];
            output << "{\"code\":\"" << runtime_detail::escape_json(error.code)
                   << "\",\"message\":\"" << runtime_detail::escape_json(error.message)
                   << "\",\"fatal\":" << (error.fatal ? "true" : "false") << '}';
        }
        output << "]}";
        const auto result = output.str();
        validate_health_json(result);
        return result;
    }

    static RuntimeManifest parse_manifest(const std::string& text) {
        using namespace runtime_detail;
        const auto root = object(JsonParser(text).parse(), "RuntimeManifest");
        exact_keys(root, {"schema_version", "runtime_id", "runtime_version", "build", "contract_versions", "promoted_components", "optional_capabilities"}, "RuntimeManifest");
        if (string(required(root, "schema_version", "RuntimeManifest"), "RuntimeManifest.schema_version") != "1.0") {
            throw RuntimeHostError("unsupported RuntimeManifest schema version");
        }
        RuntimeManifest manifest;
        manifest.runtime_id = string(required(root, "runtime_id", "RuntimeManifest"), "RuntimeManifest.runtime_id");
        manifest.runtime_version = string(required(root, "runtime_version", "RuntimeManifest"), "RuntimeManifest.runtime_version");
        if (!semver(manifest.runtime_version)) throw RuntimeHostError("RuntimeManifest.runtime_version must be semantic version");

        const auto build = object(required(root, "build", "RuntimeManifest"), "RuntimeManifest.build");
        exact_keys(build, {"platform", "compiler", "profile", "commit", "python_runtime_dependency"}, "RuntimeManifest.build");
        manifest.platform = string(required(build, "platform", "RuntimeManifest.build"), "RuntimeManifest.build.platform");
        manifest.compiler = string(required(build, "compiler", "RuntimeManifest.build"), "RuntimeManifest.build.compiler");
        manifest.profile = string(required(build, "profile", "RuntimeManifest.build"), "RuntimeManifest.build.profile");
        if (manifest.profile != "Debug" && manifest.profile != "RelWithDebInfo" && manifest.profile != "Release") {
            throw RuntimeHostError("RuntimeManifest.build.profile is invalid");
        }
        manifest.commit = string(required(build, "commit", "RuntimeManifest.build"), "RuntimeManifest.build.commit");
        if (boolean(required(build, "python_runtime_dependency", "RuntimeManifest.build"), "RuntimeManifest.build.python_runtime_dependency")) {
            throw RuntimeHostError("RuntimeManifest cannot depend on Python");
        }

        const auto versions = object(required(root, "contract_versions", "RuntimeManifest"), "RuntimeManifest.contract_versions");
        if (versions.empty()) throw RuntimeHostError("RuntimeManifest.contract_versions must not be empty");
        for (const auto& [key, value] : versions) manifest.contract_versions.emplace(key, string(value, "RuntimeManifest.contract_versions." + key));

        for (const auto& promotion : array(required(root, "promoted_components", "RuntimeManifest"), "RuntimeManifest.promoted_components")) {
            const auto item = object(promotion, "RuntimeManifest.promoted_components[]");
            exact_keys(item, {"component_id", "promotion_id", "version"}, "RuntimeManifest.promoted_components[]");
            string(required(item, "component_id", "RuntimeManifest.promoted_components[]"), "RuntimeManifest.promoted_components[].component_id");
            string(required(item, "promotion_id", "RuntimeManifest.promoted_components[]"), "RuntimeManifest.promoted_components[].promotion_id");
            string(required(item, "version", "RuntimeManifest.promoted_components[]"), "RuntimeManifest.promoted_components[].version");
        }

        std::set<std::string> seen;
        for (const auto& capability : array(required(root, "optional_capabilities", "RuntimeManifest"), "RuntimeManifest.optional_capabilities")) {
            const auto capability_id = string(capability, "RuntimeManifest.optional_capabilities[]");
            if (!seen.insert(capability_id).second) throw RuntimeHostError("RuntimeManifest.optional_capabilities must be unique");
            manifest.optional_capabilities.push_back(capability_id);
        }
        return manifest;
    }

    static CanonicalEvent parse_canonical_event(const std::string& text) {
        using namespace runtime_detail;
        const auto root = object(JsonParser(text).parse(), "CanonicalEvent");
        exact_keys(root, {"schema_version", "event_id", "source", "event_type", "occurred_at", "monotonic_ns", "received_at", "session_id", "actor_id", "context", "payload", "quality", "provenance", "privacy_class", "tags"}, "CanonicalEvent");
        if (string(required(root, "schema_version", "CanonicalEvent"), "CanonicalEvent.schema_version") != "1.0") {
            throw RuntimeHostError("unsupported CanonicalEvent schema version");
        }
        const auto& actor = required(root, "actor_id", "CanonicalEvent");
        if (!is_null(actor)) string(actor, "CanonicalEvent.actor_id");
        string(required(root, "occurred_at", "CanonicalEvent"), "CanonicalEvent.occurred_at");
        string(required(root, "received_at", "CanonicalEvent"), "CanonicalEvent.received_at");
        string(required(root, "session_id", "CanonicalEvent"), "CanonicalEvent.session_id");
        string(required(root, "privacy_class", "CanonicalEvent"), "CanonicalEvent.privacy_class");
        object(required(root, "context", "CanonicalEvent"), "CanonicalEvent.context");
        object(required(root, "payload", "CanonicalEvent"), "CanonicalEvent.payload");
        object(required(root, "quality", "CanonicalEvent"), "CanonicalEvent.quality");
        object(required(root, "provenance", "CanonicalEvent"), "CanonicalEvent.provenance");
        for (const auto& tag : array(required(root, "tags", "CanonicalEvent"), "CanonicalEvent.tags")) {
            string(tag, "CanonicalEvent.tags[]");
        }
        CanonicalEvent event;
        event.event_id = string(required(root, "event_id", "CanonicalEvent"), "CanonicalEvent.event_id");
        event.source = string(required(root, "source", "CanonicalEvent"), "CanonicalEvent.source");
        event.event_type = string(required(root, "event_type", "CanonicalEvent"), "CanonicalEvent.event_type");
        event.monotonic_ns = static_cast<std::size_t>(unsigned_number(required(root, "monotonic_ns", "CanonicalEvent"), "CanonicalEvent.monotonic_ns"));
        event.occurred_at = string(required(root, "occurred_at", "CanonicalEvent"), "CanonicalEvent.occurred_at");
        event.received_at = string(required(root, "received_at", "CanonicalEvent"), "CanonicalEvent.received_at");
        event.session_id = string(required(root, "session_id", "CanonicalEvent"), "CanonicalEvent.session_id");
        parse_iso8601_epoch(event.occurred_at);
        parse_iso8601_epoch(event.received_at);
        event.payload = text;
        return event;
    }

    static void validate_health_json(const std::string& text) {
        using namespace runtime_detail;
        const auto root = object(JsonParser(text).parse(), "RuntimeHealth");
        exact_keys(root, {"schema_version", "runtime_id", "session_id", "observed_at", "state", "event_bus", "timeline", "capabilities", "errors"}, "RuntimeHealth");
        if (string(required(root, "schema_version", "RuntimeHealth"), "RuntimeHealth.schema_version") != "1.0") throw RuntimeHostError("unsupported RuntimeHealth schema version");
        string(required(root, "runtime_id", "RuntimeHealth"), "RuntimeHealth.runtime_id");
        string(required(root, "session_id", "RuntimeHealth"), "RuntimeHealth.session_id");
        string(required(root, "observed_at", "RuntimeHealth"), "RuntimeHealth.observed_at");
        const auto runtime_state = string(required(root, "state", "RuntimeHealth"), "RuntimeHealth.state");
        if (runtime_state != "starting" && runtime_state != "ready" && runtime_state != "degraded" && runtime_state != "stopping" && runtime_state != "stopped" && runtime_state != "failed") throw RuntimeHostError("RuntimeHealth.state is invalid");

        const auto event_bus = object(required(root, "event_bus", "RuntimeHealth"), "RuntimeHealth.event_bus");
        exact_keys(event_bus, {"state", "published_events", "dead_letters"}, "RuntimeHealth.event_bus");
        const auto event_bus_state = string(required(event_bus, "state", "RuntimeHealth.event_bus"), "RuntimeHealth.event_bus.state");
        if (event_bus_state != "starting" && event_bus_state != "ready" && event_bus_state != "stopped" && event_bus_state != "failed") {
            throw RuntimeHostError("RuntimeHealth.event_bus.state is invalid");
        }
        unsigned_number(required(event_bus, "published_events", "RuntimeHealth.event_bus"), "RuntimeHealth.event_bus.published_events");
        unsigned_number(required(event_bus, "dead_letters", "RuntimeHealth.event_bus"), "RuntimeHealth.event_bus.dead_letters");

        const auto timeline = object(required(root, "timeline", "RuntimeHealth"), "RuntimeHealth.timeline");
        exact_keys(timeline, {"state", "path", "recovered_events"}, "RuntimeHealth.timeline");
        const auto timeline_state = string(required(timeline, "state", "RuntimeHealth.timeline"), "RuntimeHealth.timeline.state");
        if (timeline_state != "unavailable" && timeline_state != "ready" && timeline_state != "degraded" &&
            timeline_state != "stopped" && timeline_state != "failed") {
            throw RuntimeHostError("RuntimeHealth.timeline.state is invalid");
        }
        const auto& timeline_path = required(timeline, "path", "RuntimeHealth.timeline");
        if (!is_null(timeline_path)) string(timeline_path, "RuntimeHealth.timeline.path");
        unsigned_number(required(timeline, "recovered_events", "RuntimeHealth.timeline"), "RuntimeHealth.timeline.recovered_events");

        for (const auto& capability : array(required(root, "capabilities", "RuntimeHealth"), "RuntimeHealth.capabilities")) {
            const auto item = object(capability, "RuntimeHealth.capabilities[]");
            exact_keys(item, {"capability_id", "state", "required", "reason_code"}, "RuntimeHealth.capabilities[]");
            string(required(item, "capability_id", "RuntimeHealth.capabilities[]"), "RuntimeHealth.capabilities[].capability_id");
            const auto capability_state = string(required(item, "state", "RuntimeHealth.capabilities[]"), "RuntimeHealth.capabilities[].state");
            if (capability_state != "unknown" && capability_state != "discovered" &&
                capability_state != "calibrating" && capability_state != "available" &&
                capability_state != "degraded" && capability_state != "temporarily_unavailable" &&
                capability_state != "disabled" && capability_state != "failed" &&
                capability_state != "removed" && capability_state != "incompatible") {
                throw RuntimeHostError("RuntimeHealth.capabilities[].state is invalid");
            }
            boolean(required(item, "required", "RuntimeHealth.capabilities[]"), "RuntimeHealth.capabilities[].required");
            const auto& reason = required(item, "reason_code", "RuntimeHealth.capabilities[]");
            if (!is_null(reason)) string(reason, "RuntimeHealth.capabilities[].reason_code");
        }
        for (const auto& error : array(required(root, "errors", "RuntimeHealth"), "RuntimeHealth.errors")) {
            const auto item = object(error, "RuntimeHealth.errors[]");
            exact_keys(item, {"code", "message", "fatal"}, "RuntimeHealth.errors[]");
            string(required(item, "code", "RuntimeHealth.errors[]"), "RuntimeHealth.errors[].code");
            string(required(item, "message", "RuntimeHealth.errors[]"), "RuntimeHealth.errors[].message");
            boolean(required(item, "fatal", "RuntimeHealth.errors[]"), "RuntimeHealth.errors[].fatal");
        }
    }

    static void validate_storage_health_json(const std::string& text) {
        using namespace runtime_detail;
        const auto root = object(JsonParser(text).parse(), "StorageHealth");
        exact_keys(root, {"schema_version", "status", "quota_bytes", "user_bytes", "model_bytes",
                          "capture_suspended", "user_decision_required", "reason_code"}, "StorageHealth");
        if (string(required(root, "schema_version", "StorageHealth"), "StorageHealth.schema_version") != "1.0") {
            throw RuntimeHostError("unsupported StorageHealth schema version");
        }
        const auto status = string(required(root, "status", "StorageHealth"), "StorageHealth.status");
        if (status != "ready" && status != "degraded") throw RuntimeHostError("StorageHealth.status is invalid");
        unsigned_number(required(root, "quota_bytes", "StorageHealth"), "StorageHealth.quota_bytes");
        unsigned_number(required(root, "user_bytes", "StorageHealth"), "StorageHealth.user_bytes");
        unsigned_number(required(root, "model_bytes", "StorageHealth"), "StorageHealth.model_bytes");
        boolean(required(root, "capture_suspended", "StorageHealth"), "StorageHealth.capture_suspended");
        boolean(required(root, "user_decision_required", "StorageHealth"), "StorageHealth.user_decision_required");
        const auto& reason_value = required(root, "reason_code", "StorageHealth");
        if (!std::holds_alternative<std::string>(reason_value.value)) {
            throw RuntimeHostError("StorageHealth.reason_code must be a string");
        }
        const auto& reason = std::get<std::string>(reason_value.value);
        if (!reason.empty() && reason != "storage_quota_exceeded" && reason != "storage_recovery_required") {
            throw RuntimeHostError("StorageHealth.reason_code is invalid");
        }
    }

private:
    struct RuntimeError {
        std::string code;
        std::string message;
        bool fatal{false};
    };

    static CognitiveSnapshotV2 parse_cognitive_snapshot_v2(
        const std::string& json) {
        using namespace runtime_detail;
        if (!CognitiveSnapshotV2::serialized_checksum_valid(json)) {
            throw RuntimeHostError("cognitive snapshot checksum is invalid");
        }
        const auto root = object(JsonParser(json).parse(), "CognitiveSnapshotV2");
        exact_keys(root,
                   {"captured_at", "captured_epoch_seconds",
                    "configuration_fingerprint", "last_applied_event_id",
                    "schema_version", "state", "checksum"},
                   "CognitiveSnapshotV2");
        CognitiveSnapshotV2 snapshot;
        snapshot.schema_version = string(
            required(root, "schema_version", "CognitiveSnapshotV2"),
            "CognitiveSnapshotV2.schema_version");
        snapshot.captured_at = string(
            required(root, "captured_at", "CognitiveSnapshotV2"),
            "CognitiveSnapshotV2.captured_at");
        snapshot.captured_epoch_seconds = finite_number(
            required(root, "captured_epoch_seconds", "CognitiveSnapshotV2"),
            "CognitiveSnapshotV2.captured_epoch_seconds");
        snapshot.configuration_fingerprint = string(
            required(root, "configuration_fingerprint", "CognitiveSnapshotV2"),
            "CognitiveSnapshotV2.configuration_fingerprint");
        snapshot.last_applied_event_id = string(
            required(root, "last_applied_event_id", "CognitiveSnapshotV2"),
            "CognitiveSnapshotV2.last_applied_event_id");
        snapshot.checksum = string(
            required(root, "checksum", "CognitiveSnapshotV2"),
            "CognitiveSnapshotV2.checksum");

        const auto state = object(
            required(root, "state", "CognitiveSnapshotV2"),
            "CognitiveSnapshotV2.state");
        exact_keys(state,
                   {"coordinator", "fragments", "required_provider_ids",
                    "schema_version"},
                   "CognitiveSnapshotV2.state");
        snapshot.state.schema_version = string(
            required(state, "schema_version", "CognitiveSnapshotV2.state"),
            "CognitiveSnapshotV2.state.schema_version");

        const auto coordinator = object(
            required(state, "coordinator", "CognitiveSnapshotV2.state"),
            "CognitiveSnapshotV2.state.coordinator");
        exact_keys(coordinator,
                   {"policy_id", "schema_version", "seen_event_ids"},
                   "CognitiveSnapshotV2.state.coordinator");
        snapshot.state.coordinator.schema_version = string(
            required(coordinator, "schema_version",
                     "CognitiveSnapshotV2.state.coordinator"),
            "CognitiveSnapshotV2.state.coordinator.schema_version");
        snapshot.state.coordinator.policy_id = string(
            required(coordinator, "policy_id",
                     "CognitiveSnapshotV2.state.coordinator"),
            "CognitiveSnapshotV2.state.coordinator.policy_id");
        for (const auto& item : array(
                 required(coordinator, "seen_event_ids",
                          "CognitiveSnapshotV2.state.coordinator"),
                 "CognitiveSnapshotV2.state.coordinator.seen_event_ids")) {
            snapshot.state.coordinator.seen_event_ids.push_back(
                string(item, "CognitiveSnapshotV2.state.coordinator.seen_event_ids[]"));
        }
        for (const auto& item : array(
                 required(state, "required_provider_ids",
                          "CognitiveSnapshotV2.state"),
                 "CognitiveSnapshotV2.state.required_provider_ids")) {
            snapshot.state.required_provider_ids.push_back(
                string(item,
                       "CognitiveSnapshotV2.state.required_provider_ids[]"));
        }
        for (const auto& item : array(
                 required(state, "fragments", "CognitiveSnapshotV2.state"),
                 "CognitiveSnapshotV2.state.fragments")) {
            const auto fragment_value = object(
                item, "CognitiveSnapshotV2.state.fragments[]");
            exact_keys(fragment_value,
                       {"entries", "provider_id", "schema_version",
                        "state_schema_version"},
                       "CognitiveSnapshotV2.state.fragments[]");
            contracts::CognitiveStateFragmentV1 fragment;
            fragment.schema_version = string(
                required(fragment_value, "schema_version", "fragment"),
                "fragment.schema_version");
            fragment.provider_id = string(
                required(fragment_value, "provider_id", "fragment"),
                "fragment.provider_id");
            fragment.state_schema_version = string(
                required(fragment_value, "state_schema_version", "fragment"),
                "fragment.state_schema_version");
            const auto entries = object(
                required(fragment_value, "entries", "fragment"),
                "fragment.entries");
            for (const auto& [key, value] : entries) {
                if (key.empty()) {
                    throw RuntimeHostError("fragment entry key must not be empty");
                }
                fragment.entries.emplace(
                    key, string_allow_empty(value, "fragment.entries value"));
            }
            snapshot.state.fragments.push_back(std::move(fragment));
        }
        if (!snapshot.valid() || snapshot.to_json() != json) {
            throw RuntimeHostError(
                "cognitive snapshot is invalid or non-canonical");
        }
        return snapshot;
    }

    contracts::CognitiveCycleInputV1 make_cycle_input(
        const CanonicalEvent& event, bool replay_mode) const {
        contracts::CognitiveCycleInputV1 input;
        input.correlation_id = event.event_id;
        input.event_id = event.event_id;
        input.source = event.source;
        input.event_type = event.event_type;
        input.session_id = event.session_id.empty() ? config_.session_id : event.session_id;
        input.occurred_at = event.occurred_at.empty() ? config_.observed_at : event.occurred_at;
        input.modality = event.source;
        input.time_basis = event.occurred_at.empty()
            ? "received_fallback"
            : "source_occurred";
        try {
            input.epoch_seconds = runtime_detail::parse_iso8601_epoch(input.occurred_at);
        } catch (const RuntimeHostError&) {
            input.epoch_seconds = std::numeric_limits<double>::quiet_NaN();
        }
        input.replay_mode = replay_mode;
        return input;
    }

    void persist(const CanonicalEvent& event) {
        std::lock_guard lock(mutex_);
        try {
            if (!timeline_) throw RuntimeHostError("timeline is unavailable");
            const auto reservation = static_cast<std::uint64_t>(event.payload.size()) + 512;
            if (!storage_quota_.begin_write(reservation)) {
                add_error_locked("storage_quota_exceeded", "new capture suspended until the user resolves local storage quota", false);
                state_ = RuntimeState::degraded;
                return;
            }
            const auto result = timeline_->append(event, TimelineMetadata{config_.session_id, event.source, event.event_id});
            if (result == AppendResult::accepted) {
                storage_quota_.commit_write(StorageUsage{.payload_bytes = reservation});
                ++published_events_;
                refresh_storage_usage_locked();
                if (storage_quota_.health().status == StorageStatus::degraded) {
                    add_error_locked("storage_quota_exceeded", "local storage quota requires user decision", false);
                    state_ = RuntimeState::degraded;
                }
            } else {
                storage_quota_.abort_write(reservation);
            }
        } catch (const std::exception& error) {
            try {
                storage_quota_.abort_write(static_cast<std::uint64_t>(event.payload.size()) + 512);
            } catch (...) {}
            add_error_locked("timeline_append_failed", error.what(), true);
            state_ = RuntimeState::failed;
        }
    }

    void recover_cognitive_state() {
        if (!coordinator_ || !state_manager_ || !timeline_) return;
        std::optional<CognitiveSnapshotV2> selected;
        if (config_.enable_cognitive_snapshots &&
            LocalDataProtection::available()) {
            const auto observed_epoch =
                runtime_detail::parse_iso8601_epoch(config_.observed_at);
            for (const auto& record : timeline_->load_snapshot_records()) {
                try {
                    const auto plaintext = LocalDataProtection::unprotect(
                        record.encrypted_payload);
                    const std::string json(plaintext.begin(), plaintext.end());
                    auto candidate = parse_cognitive_snapshot_v2(json);
                    const auto age = observed_epoch -
                        candidate.captured_epoch_seconds;
                    if (candidate.configuration_fingerprint !=
                        configuration_fingerprint_) {
                        recovery_rejections_.push_back(
                            "configuration_fingerprint_mismatch");
                        continue;
                    }
                    if (age < -1.0 ||
                        age > static_cast<double>(
                                  config_.cognitive_snapshot_max_age.count())) {
                        recovery_rejections_.push_back("snapshot_expired");
                        continue;
                    }
                    if (!timeline_->contains_event(
                            candidate.last_applied_event_id) ||
                        !std::binary_search(
                            candidate.state.coordinator.seen_event_ids.begin(),
                            candidate.state.coordinator.seen_event_ids.end(),
                            candidate.last_applied_event_id)) {
                        recovery_rejections_.push_back("snapshot_cursor_invalid");
                        continue;
                    }
                    if (!state_manager_->restore(
                            candidate.state,
                            candidate.last_applied_event_id)) {
                        recovery_rejections_.push_back(
                            "snapshot_state_restore_failed");
                        continue;
                    }
                    selected = std::move(candidate);
                    break;
                } catch (...) {
                    // Corrupt, incompatible, expired, or undecryptable snapshots
                    // fall back to the previous record and then cold replay.
                    recovery_rejections_.push_back(
                        "snapshot_decode_or_contract_invalid");
                }
            }
        }

        recovery_source_ = selected ? "snapshot_replay" : "cold_replay";

        coordinator_->start();
        const auto events = selected
            ? timeline_->replay_from(selected->last_applied_event_id)
            : timeline_->replay();
        recovered_events_ = 0;
        for (const auto& event : events) {
            if (event.event_type == "cognitive.cycle.result") continue;
            const auto receipt =
                coordinator_->enqueue_input(make_cycle_input(event, true));
            if (receipt.status == EnqueueStatusV1::accepted) {
                ++recovered_events_;
                coordinator_->wait_idle();
            }
        }
    }

    static std::size_t count_replayable(
        const std::vector<CanonicalEvent>& events) {
        return static_cast<std::size_t>(std::count_if(
            events.begin(), events.end(), [](const auto& event) {
                return event.event_type != "cognitive.cycle.result";
            }));
    }

    void on_cycle_committed(
        const contracts::CognitiveCycleInputV1& input) {
        if (input.replay_mode) return;
        bool due = false;
        {
            std::lock_guard lock(snapshot_mutex_);
            last_committed_input_ = input;
            ++live_committed_events_;
            due = config_.enable_cognitive_snapshots &&
                live_committed_events_ %
                    config_.cognitive_snapshot_interval_events == 0;
        }
        if (due) (void)submit_snapshot(input);
    }

    bool submit_snapshot(
        const contracts::CognitiveCycleInputV1& input) {
        if (!config_.enable_cognitive_snapshots || input.replay_mode ||
            !state_manager_ || !snapshot_writer_) {
            return false;
        }
        const auto bundle = state_manager_->capture_bundle(input.event_id);
        if (!bundle) return false;
        CognitiveSnapshotWriteRequest request;
        request.captured_at = input.occurred_at;
        request.captured_epoch_seconds = input.epoch_seconds;
        request.configuration_fingerprint = configuration_fingerprint_;
        request.last_applied_event_id = input.event_id;
        request.state = *bundle;
        request.created_at_ns = static_cast<std::int64_t>(
            input.epoch_seconds * 1'000'000'000.0);
        return snapshot_writer_->submit(std::move(request));
    }

    void refresh_storage_usage_locked() {
        StorageUsage usage;
        const auto include_size = [](const std::string& path, std::uint64_t& target) {
            std::error_code error;
            if (std::filesystem::is_regular_file(path, error)) target = std::filesystem::file_size(path, error);
            if (error) target = 0;
        };
        include_size(config_.timeline_path, usage.database_bytes);
        include_size(config_.timeline_path + "-wal", usage.wal_bytes);
        include_size(config_.timeline_path + "-shm", usage.index_bytes);
        storage_quota_.set_usage(usage);
    }

    void add_error_locked(std::string code, std::string message, bool fatal) {
        errors_.push_back({std::move(code), std::move(message), fatal});
    }

    RuntimeConfig config_;
    mutable std::mutex mutex_;
    RuntimeState state_{RuntimeState::stopped};
    std::optional<RuntimeManifest> manifest_;
    CapabilityRegistry capability_registry_;
    std::shared_ptr<CognitiveCoordinator> coordinator_;
    std::unique_ptr<CognitiveStateManager> state_manager_;
    std::unique_ptr<AsyncCognitiveSnapshotWriter> snapshot_writer_;
    std::shared_ptr<EventBus> event_bus_;
    std::shared_ptr<TimelineStore> timeline_;
    StorageQuotaController storage_quota_;
    std::vector<RuntimeError> errors_;
    std::size_t published_events_{0};
    std::size_t recovered_events_{0};
    std::string configuration_fingerprint_;
    mutable std::mutex snapshot_mutex_;
    std::optional<contracts::CognitiveCycleInputV1> last_committed_input_;
    std::size_t live_committed_events_{0};
    std::string recovery_source_{"not_started"};
    std::vector<std::string> recovery_rejections_;
};

}  // namespace eu_digital
