#pragma once

#include "core/capability_runtime.hpp"
#include "core/digest.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace eu_digital {

inline constexpr const char* SUGGESTION_SCHEMA_VERSION = "1.0";
inline constexpr const char* SUGGESTION_POLICY_ID = "suggestive_orchestration_v1";
inline constexpr const char* SUGGESTION_POLICY_VERSION = "1.0";
inline constexpr const char* SUGGESTION_BASELINE_POLICY_ID = "fixed_delivery_v0";
inline constexpr const char* SUGGESTION_CREATED_BY = "suggestion_orchestrator.local.v1";
inline constexpr const char* SUGGESTION_NAMESPACE = "b8f71e23-c4d9-4a1f-9e02-3d7a2f1c8b45";
inline constexpr const char* SUGGESTION_HYPOTHESIS =
    "suggestive orchestration with evidence, budget, cooldown and suppression "
    "reduces unjustified interruptions and improves correction rate versus "
    "fixed delivery";
inline constexpr const char* SUGGESTION_ABLATION =
    "disable metacognition, budget, cooldown, redundancy suppression, or "
    "switch to fixed_delivery_v0 baseline";
inline constexpr const char* SUGGESTION_FALSIFICATION =
    "correction rate worsens, interruptions exceed policy limits, evidence "
    "or explanation is missing, or any suggestion executes an action";

class SuggestionOrchestratorError : public std::invalid_argument {
public:
    explicit SuggestionOrchestratorError(const std::string& message)
        : std::invalid_argument(message) {}
};

enum class SuggestionFeedback { correct, defer, silence };

// --- JSON helpers (self-contained, no dependency on metacognition module) ---

namespace suggestion_detail {

inline std::string json_escape(const std::string& value) {
    std::ostringstream output;
    for (const auto character : value) {
        switch (character) {
        case '"': output << "\\\""; break;
        case '\\': output << "\\\\"; break;
        case '\b': output << "\\b"; break;
        case '\f': output << "\\f"; break;
        case '\n': output << "\\n"; break;
        case '\r': output << "\\r"; break;
        case '\t': output << "\\t"; break;
        default: output << character; break;
        }
    }
    return output.str();
}

inline std::string json_string(const std::string& value) {
    return "\"" + json_escape(value) + "\"";
}

inline std::string json_number(double value) {
    if (!std::isfinite(value))
        throw SuggestionOrchestratorError("non-finite number cannot be serialized");
    std::array<char, 64> buffer{};
    const auto conversion = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
    if (conversion.ec != std::errc{})
        throw SuggestionOrchestratorError("number cannot be formatted");
    auto result = std::string(buffer.data(), conversion.ptr);
    if (result.find_first_of(".eE") == std::string::npos) result += ".0";
    return result;
}

inline std::string json_array(const std::vector<std::string>& values) {
    std::ostringstream output;
    output << '[';
    for (std::size_t index = 0; index < values.size(); ++index) {
        if (index) output << ',';
        output << json_string(values[index]);
    }
    output << ']';
    return output.str();
}

inline void required_string(const std::string& value, const char* name) {
    if (value.empty())
        throw SuggestionOrchestratorError(std::string(name) + " must be a non-empty string");
}

inline void validate_probability(double value, const char* name) {
    if (!std::isfinite(value) || value < 0.0 || value > 1.0)
        throw SuggestionOrchestratorError(std::string(name) + " must be between zero and one");
}

inline std::int64_t days_from_civil(int year, unsigned month, unsigned day) {
    year -= month <= 2;
    const int era = (year >= 0 ? year : year - 399) / 400;
    const unsigned year_of_era = static_cast<unsigned>(year - era * 400);
    const unsigned day_of_year = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
    const unsigned day_of_era = year_of_era * 365 + year_of_era / 4 - year_of_era / 100 + day_of_year;
    return static_cast<std::int64_t>(era) * 146097 + static_cast<std::int64_t>(day_of_era) - 719468;
}

inline double parse_timestamp(const std::string& value) {
    if (value.size() < 20 || value[4] != '-' || value[7] != '-' || value[10] != 'T')
        throw SuggestionOrchestratorError("timestamp must be a valid ISO-8601 string");
    const auto number = [&](std::size_t offset, std::size_t length) {
        try { return std::stoi(value.substr(offset, length)); }
        catch (const std::exception&) {
            throw SuggestionOrchestratorError("timestamp must be a valid ISO-8601 string");
        }
    };
    const int year = number(0, 4);
    const unsigned month = static_cast<unsigned>(number(5, 2));
    const unsigned day = static_cast<unsigned>(number(8, 2));
    const int hour = number(11, 2);
    const int minute = number(14, 2);
    const int second = number(17, 2);
    const auto zone_start = value.find_first_of("Z+-", 19);
    if (zone_start == std::string::npos)
        throw SuggestionOrchestratorError("timestamp must include timezone");
    double fraction = 0.0;
    if (zone_start > 19) {
        try { fraction = std::stod("0" + value.substr(19, zone_start - 19)); }
        catch (const std::exception&) {
            throw SuggestionOrchestratorError("timestamp must be a valid ISO-8601 string");
        }
    }
    int offset_seconds = 0;
    if (value[zone_start] != 'Z') {
        if (value.size() < zone_start + 6 || value[zone_start + 3] != ':')
            throw SuggestionOrchestratorError("timestamp timezone is invalid");
        const int sign = value[zone_start] == '-' ? -1 : 1;
        offset_seconds = sign * (number(zone_start + 1, 2) * 3600 + number(zone_start + 4, 2) * 60);
    }
    return static_cast<double>(days_from_civil(static_cast<int>(year), month, day) * 86400LL +
                               hour * 3600 + minute * 60 + second - offset_seconds) + fraction;
}

inline std::string format_timestamp(double epoch) {
    const auto seconds = static_cast<std::int64_t>(std::floor(epoch));
    const auto days = seconds / 86400 - (seconds < 0 && seconds % 86400 != 0 ? 1 : 0);
    const auto day_seconds = seconds - days * 86400;
    std::int64_t z = days + 719468;
    const std::int64_t era = (z >= 0 ? z : z - 146096) / 146097;
    const auto day_of_era = static_cast<unsigned>(z - era * 146097);
    const auto year_of_era = (day_of_era - day_of_era / 1460 + day_of_era / 36524 - day_of_era / 146096) / 365;
    std::int64_t year = static_cast<std::int64_t>(year_of_era) + era * 400;
    const auto day_of_year = day_of_era - (365 * year_of_era + year_of_era / 4 - year_of_era / 100);
    const auto month_part = (5 * day_of_year + 2) / 153;
    const auto day = day_of_year - (153 * month_part + 2) / 5 + 1;
    const auto month = month_part + (month_part < 10 ? 3 : -9);
    year += month <= 2;
    const auto hour = day_seconds / 3600;
    const auto minute = (day_seconds % 3600) / 60;
    const auto second = day_seconds % 60;
    std::ostringstream output;
    output << std::setfill('0') << std::setw(4) << year << '-' << std::setw(2) << month << '-' << std::setw(2) << day
           << 'T' << std::setw(2) << hour << ':' << std::setw(2) << minute << ':' << std::setw(2) << second << "+00:00";
    return output.str();
}

inline std::string normalize_timestamp(const std::string& value) {
    return format_timestamp(parse_timestamp(value));
}

}  // namespace suggestion_detail

// --- Core data structures ---

struct SuggestionPolicy {
    std::string policy_id{SUGGESTION_POLICY_ID};
    std::string policy_version{SUGGESTION_POLICY_VERSION};
    int max_per_window{3};
    double window_seconds{900.0};       // 15 min
    double cooldown_seconds{300.0};     // 5 min
    double correction_cooldown_seconds{1800.0};  // 30 min after correction
    int max_per_day{8};
    double min_confidence{0.15};
    double min_information_gain{0.05};
    bool redundancy_suppression{true};
    bool budget_enabled{true};
    bool cooldown_enabled{true};

    void validate() const {
        suggestion_detail::required_string(policy_id, "policy_id");
        suggestion_detail::required_string(policy_version, "policy_version");
        if (max_per_window <= 0)
            throw SuggestionOrchestratorError("max_per_window must be positive");
        if (max_per_day <= 0)
            throw SuggestionOrchestratorError("max_per_day must be positive");
        for (const auto value : {window_seconds, cooldown_seconds, correction_cooldown_seconds}) {
            if (!std::isfinite(value) || value < 0.0)
                throw SuggestionOrchestratorError("time policy values must be finite and non-negative");
        }
        suggestion_detail::validate_probability(min_confidence, "min_confidence");
        suggestion_detail::validate_probability(min_information_gain, "min_information_gain");
    }

    std::string fingerprint() const {
        validate();
        std::ostringstream value;
        value << "{\"budget_enabled\":" << (budget_enabled ? "true" : "false")
              << ",\"cooldown_enabled\":" << (cooldown_enabled ? "true" : "false")
              << ",\"cooldown_seconds\":" << suggestion_detail::json_number(cooldown_seconds)
              << ",\"correction_cooldown_seconds\":" << suggestion_detail::json_number(correction_cooldown_seconds)
              << ",\"max_per_day\":" << max_per_day
              << ",\"max_per_window\":" << max_per_window
              << ",\"min_confidence\":" << suggestion_detail::json_number(min_confidence)
              << ",\"min_information_gain\":" << suggestion_detail::json_number(min_information_gain)
              << ",\"policy_id\":" << suggestion_detail::json_string(policy_id)
              << ",\"policy_version\":" << suggestion_detail::json_string(policy_version)
              << ",\"redundancy_suppression\":" << (redundancy_suppression ? "true" : "false")
              << ",\"window_seconds\":" << suggestion_detail::json_number(window_seconds) << '}';
        return digest::hex(digest::sha256(value.str())).substr(0, 16);
    }
};

struct SuggestionEvidence {
    std::string hypothesis_id;
    double confidence{0.0};
    double information_gain{0.0};
    std::vector<std::string> evidence_ids;
    std::string reason;

    void validate() const {
        suggestion_detail::required_string(hypothesis_id, "hypothesis_id");
        suggestion_detail::validate_probability(confidence, "confidence");
        if (!std::isfinite(information_gain) || information_gain < 0.0)
            throw SuggestionOrchestratorError("information_gain must be finite and non-negative");
        suggestion_detail::required_string(reason, "reason");
        if (evidence_ids.empty())
            throw SuggestionOrchestratorError("suggestion must carry at least one evidence reference");
        for (const auto& ref : evidence_ids)
            suggestion_detail::required_string(ref, "evidence_id");
    }
};

struct SuggestionDecision {
    std::string decision_id;
    std::string schema_version{SUGGESTION_SCHEMA_VERSION};
    std::string policy_id{SUGGESTION_POLICY_ID};
    std::string policy_version{SUGGESTION_POLICY_VERSION};
    std::vector<std::string> evidence_ids;
    std::string hypothesis_id;
    double confidence{0.0};
    double information_gain{0.0};
    std::string reason;
    bool suppressed{false};
    std::optional<std::string> suppression_reason;
    int budget_before{0};
    int budget_after{0};
    double cooldown_remaining_seconds{0.0};
    bool override_active{false};
    bool action_proposed{false};  // always false per SPEC-043
    std::string created_at;

    void validate() const {
        suggestion_detail::required_string(decision_id, "decision_id");
        if (schema_version != SUGGESTION_SCHEMA_VERSION)
            throw SuggestionOrchestratorError("unsupported suggestion schema version");
        suggestion_detail::required_string(policy_id, "policy_id");
        suggestion_detail::required_string(policy_version, "policy_version");
        suggestion_detail::validate_probability(confidence, "confidence");
        if (!std::isfinite(information_gain) || information_gain < 0.0)
            throw SuggestionOrchestratorError("information_gain must be finite and non-negative");
        suggestion_detail::required_string(reason, "reason");
        if (evidence_ids.empty())
            throw SuggestionOrchestratorError("suggestion must carry evidence");
        for (const auto& ref : evidence_ids)
            suggestion_detail::required_string(ref, "evidence_id");
        if (action_proposed)
            throw SuggestionOrchestratorError("SPEC-043 prohibits action proposals");
        if (suppressed != suppression_reason.has_value())
            throw SuggestionOrchestratorError("suppression flag must match suppression_reason presence");
        if (suppression_reason && suppression_reason->empty())
            throw SuggestionOrchestratorError("suppression_reason cannot be empty");
        suggestion_detail::required_string(created_at, "created_at");
    }

    std::string to_json() const {
        validate();
        std::ostringstream output;
        output << "{\"action_proposed\":false"
               << ",\"budget_after\":" << budget_after
               << ",\"budget_before\":" << budget_before
               << ",\"confidence\":" << suggestion_detail::json_number(confidence)
               << ",\"cooldown_remaining_seconds\":" << suggestion_detail::json_number(cooldown_remaining_seconds)
               << ",\"created_at\":" << suggestion_detail::json_string(created_at)
               << ",\"decision_id\":" << suggestion_detail::json_string(decision_id)
               << ",\"evidence_ids\":" << suggestion_detail::json_array(evidence_ids)
               << ",\"hypothesis_id\":" << suggestion_detail::json_string(hypothesis_id)
               << ",\"information_gain\":" << suggestion_detail::json_number(information_gain)
               << ",\"override_active\":" << (override_active ? "true" : "false")
               << ",\"policy_id\":" << suggestion_detail::json_string(policy_id)
               << ",\"policy_version\":" << suggestion_detail::json_string(policy_version)
               << ",\"reason\":" << suggestion_detail::json_string(reason)
               << ",\"schema_version\":" << suggestion_detail::json_string(schema_version)
               << ",\"suppressed\":" << (suppressed ? "true" : "false")
               << ",\"suppression_reason\":" << (suppression_reason ? suggestion_detail::json_string(*suppression_reason) : "null")
               << '}';
        return output.str();
    }
};

struct SuggestionFeedbackRecord {
    std::string feedback_id;
    std::string decision_id;
    SuggestionFeedback action{SuggestionFeedback::silence};
    std::optional<std::string> correction;
    std::string occurred_at;

    void validate() const {
        suggestion_detail::required_string(feedback_id, "feedback_id");
        suggestion_detail::required_string(decision_id, "decision_id");
        suggestion_detail::required_string(occurred_at, "occurred_at");
        if (action == SuggestionFeedback::correct) {
            if (!correction || correction->empty())
                throw SuggestionOrchestratorError("correct feedback requires a correction");
        } else if (correction) {
            throw SuggestionOrchestratorError("only correct feedback accepts a correction");
        }
    }

    static std::string action_string(SuggestionFeedback value) {
        switch (value) {
        case SuggestionFeedback::correct: return "correct";
        case SuggestionFeedback::defer: return "defer";
        case SuggestionFeedback::silence: return "silence";
        }
        throw SuggestionOrchestratorError("unsupported suggestion feedback action");
    }

    std::string to_json() const {
        validate();
        std::ostringstream output;
        output << "{\"action\":" << suggestion_detail::json_string(action_string(action))
               << ",\"correction\":" << (correction ? suggestion_detail::json_string(*correction) : "null")
               << ",\"decision_id\":" << suggestion_detail::json_string(decision_id)
               << ",\"feedback_id\":" << suggestion_detail::json_string(feedback_id)
               << ",\"occurred_at\":" << suggestion_detail::json_string(occurred_at) << '}';
        return output.str();
    }
};

// --- Orchestrator engine ---

class SuggestionOrchestrator {
public:
    explicit SuggestionOrchestrator(SuggestionPolicy policy = {}, bool model_available = false)
        : policy_(std::move(policy)), model_available_(model_available) {
        policy_.validate();
    }

    const SuggestionPolicy& policy() const { return policy_; }
    bool model_available() const { return model_available_; }
    void set_model_available(bool available) { model_available_ = available; }

    const std::vector<SuggestionDecision>& decisions() const { return decisions_; }
    const std::vector<SuggestionFeedbackRecord>& feedback_history() const { return feedback_history_; }

    /// Evaluate a suggestion candidate from evidence.
    /// This never executes actions — it only decides whether to suggest.
    SuggestionDecision evaluate(const SuggestionEvidence& evidence, const std::string& now) {
        evidence.validate();
        const auto moment = suggestion_detail::normalize_timestamp(now);
        const auto epoch = suggestion_detail::parse_timestamp(moment);

        const int budget_before = remaining_budget(epoch);
        const auto suppression = compute_suppression(evidence, epoch, moment);

        const int budget_after = suppression ? budget_before : std::max(0, budget_before - 1);
        const double cooldown_remaining = compute_cooldown_remaining(evidence.hypothesis_id, epoch);

        const auto decision_id = digest::uuid5(
            SUGGESTION_NAMESPACE,
            evidence.hypothesis_id + ":" + moment + ":" +
            std::to_string(decisions_.size()) + ":" + policy_.fingerprint());

        SuggestionDecision decision{
            decision_id,
            SUGGESTION_SCHEMA_VERSION,
            policy_.policy_id,
            policy_.policy_version,
            evidence.evidence_ids,
            evidence.hypothesis_id,
            evidence.confidence,
            evidence.information_gain,
            evidence.reason,
            suppression.has_value(),
            suppression,
            budget_before,
            budget_after,
            cooldown_remaining,
            false,  // override_active
            false,  // action_proposed — SPEC-043 invariant
            moment
        };

        decision.validate();
        decisions_.push_back(decision);

        if (!suppression) {
            delivered_at_.push_back(epoch);
            delivered_today_.push_back(epoch);
            delivered_fingerprints_.insert(evidence.hypothesis_id);
            if (policy_.cooldown_enabled) {
                cooldown_until_[evidence.hypothesis_id] =
                    suggestion_detail::format_timestamp(epoch + policy_.cooldown_seconds);
            }
        }

        return decision;
    }

    /// Record user feedback on a delivered suggestion.
    SuggestionFeedbackRecord record_feedback(
        const std::string& decision_id, SuggestionFeedback action,
        std::optional<std::string> correction, const std::string& now) {
        const auto* decision = find_decision(decision_id);
        if (!decision)
            throw SuggestionOrchestratorError("decision not found");
        if (decision->suppressed)
            throw SuggestionOrchestratorError("cannot provide feedback on suppressed suggestions");

        const auto moment = suggestion_detail::normalize_timestamp(now);
        const auto feedback_id = digest::uuid5(
            SUGGESTION_NAMESPACE,
            decision_id + ":" + moment + ":" +
            SuggestionFeedbackRecord::action_string(action));

        SuggestionFeedbackRecord record{
            feedback_id, decision_id, action, std::move(correction), moment};
        record.validate();

        if (action == SuggestionFeedback::correct) {
            correction_count_[decision->hypothesis_id] =
                (correction_count_.count(decision->hypothesis_id)
                     ? correction_count_.at(decision->hypothesis_id) : 0) + 1;
            if (policy_.cooldown_enabled) {
                cooldown_until_[decision->hypothesis_id] =
                    suggestion_detail::format_timestamp(
                        suggestion_detail::parse_timestamp(moment) +
                        policy_.correction_cooldown_seconds);
            }
        }

        feedback_history_.push_back(record);
        ++total_feedback_count_;
        if (action == SuggestionFeedback::correct) ++accepted_count_;
        if (action == SuggestionFeedback::silence) ++silenced_count_;

        return record;
    }

    /// Explicit degradation when model is absent — SPEC-043 requirement.
    SuggestionDecision evaluate_without_model(
        const SuggestionEvidence& evidence, const std::string& now) {
        if (model_available_)
            throw SuggestionOrchestratorError("model is available; use evaluate() instead");

        evidence.validate();
        const auto moment = suggestion_detail::normalize_timestamp(now);

        const auto decision_id = digest::uuid5(
            SUGGESTION_NAMESPACE,
            "no-model:" + evidence.hypothesis_id + ":" + moment + ":" +
            std::to_string(decisions_.size()));

        SuggestionDecision decision{
            decision_id,
            SUGGESTION_SCHEMA_VERSION,
            policy_.policy_id,
            policy_.policy_version,
            evidence.evidence_ids,
            evidence.hypothesis_id,
            evidence.confidence,
            evidence.information_gain,
            evidence.reason + " [model absent: explicit degradation, no semantic fallback]",
            true,   // suppressed
            std::string("model_absent"),
            remaining_budget(suggestion_detail::parse_timestamp(moment)),
            remaining_budget(suggestion_detail::parse_timestamp(moment)),
            0.0,
            false,  // override_active
            false,  // action_proposed
            moment
        };

        decision.validate();
        decisions_.push_back(decision);
        return decision;
    }

    /// Metrics snapshot for verification.
    std::string metrics_json() const {
        const int delivered = count_delivered();
        const int suppressed = static_cast<int>(decisions_.size()) - delivered;
        std::ostringstream output;
        output << "{\"ablation\":" << suggestion_detail::json_string(SUGGESTION_ABLATION)
               << ",\"accepted\":" << accepted_count_
               << ",\"delivered\":" << delivered
               << ",\"falsification\":" << suggestion_detail::json_string(SUGGESTION_FALSIFICATION)
               << ",\"hypothesis\":" << suggestion_detail::json_string(SUGGESTION_HYPOTHESIS)
               << ",\"policy_fingerprint\":" << suggestion_detail::json_string(policy_.fingerprint())
               << ",\"policy_id\":" << suggestion_detail::json_string(policy_.policy_id)
               << ",\"policy_version\":" << suggestion_detail::json_string(policy_.policy_version)
               << ",\"silenced\":" << silenced_count_
               << ",\"suppressed\":" << suppressed
               << ",\"total_decisions\":" << decisions_.size()
               << ",\"total_feedback\":" << total_feedback_count_ << '}';
        return output.str();
    }

private:
    int remaining_budget(double now_epoch) const {
        if (!policy_.budget_enabled) return policy_.max_per_window;

        // Window budget (max_per_window in window_seconds)
        const double window_start = now_epoch - policy_.window_seconds;
        int window_count = 0;
        for (const auto& epoch : delivered_at_) {
            if (epoch >= window_start) ++window_count;
        }
        const int window_remaining = std::max(0, policy_.max_per_window - window_count);

        // Daily budget
        const double day_start = now_epoch - 86400.0;
        int day_count = 0;
        for (const auto& epoch : delivered_today_) {
            if (epoch >= day_start) ++day_count;
        }
        const int day_remaining = std::max(0, policy_.max_per_day - day_count);

        return std::min(window_remaining, day_remaining);
    }

    double compute_cooldown_remaining(const std::string& hypothesis_id, double now_epoch) const {
        if (!policy_.cooldown_enabled) return 0.0;
        const auto found = cooldown_until_.find(hypothesis_id);
        if (found == cooldown_until_.end()) return 0.0;
        const double until_epoch = suggestion_detail::parse_timestamp(found->second);
        return std::max(0.0, until_epoch - now_epoch);
    }

    std::optional<std::string> compute_suppression(
        const SuggestionEvidence& evidence, double now_epoch, const std::string& /*moment*/) const {

        // Budget exhausted
        if (policy_.budget_enabled && remaining_budget(now_epoch) <= 0)
            return std::string("budget_exhausted");

        // Cooldown active
        if (policy_.cooldown_enabled) {
            const auto found = cooldown_until_.find(evidence.hypothesis_id);
            if (found != cooldown_until_.end()) {
                const double until_epoch = suggestion_detail::parse_timestamp(found->second);
                if (now_epoch < until_epoch) return std::string("cooldown_active");
            }
        }

        // Confidence below minimum
        if (evidence.confidence < policy_.min_confidence)
            return std::string("confidence_below_minimum");

        // Information gain below minimum
        if (evidence.information_gain < policy_.min_information_gain)
            return std::string("information_gain_below_minimum");

        // Redundancy: same hypothesis already delivered
        if (policy_.redundancy_suppression &&
            delivered_fingerprints_.count(evidence.hypothesis_id)) {
            // Check if a correction was provided (correction resets redundancy)
            if (!correction_count_.count(evidence.hypothesis_id))
                return std::string("redundant_hypothesis");
        }

        return std::nullopt;
    }

    const SuggestionDecision* find_decision(const std::string& decision_id) const {
        for (const auto& decision : decisions_) {
            if (decision.decision_id == decision_id) return &decision;
        }
        return nullptr;
    }

    int count_delivered() const {
        int count = 0;
        for (const auto& decision : decisions_) {
            if (!decision.suppressed) ++count;
        }
        return count;
    }

    SuggestionPolicy policy_;
    bool model_available_{false};
    std::vector<SuggestionDecision> decisions_;
    std::vector<SuggestionFeedbackRecord> feedback_history_;
    std::vector<double> delivered_at_;
    std::vector<double> delivered_today_;
    std::set<std::string> delivered_fingerprints_;
    std::map<std::string, std::string> cooldown_until_;
    std::map<std::string, int> correction_count_;
    int total_feedback_count_{0};
    int accepted_count_{0};
    int silenced_count_{0};
};

class SuggestionOrchestratorPlugin final : public CapabilityPlugin {
public:
    SuggestionOrchestratorPlugin() {
        descriptor_.capability_id = "cognition.suggestion_orchestrator";
        descriptor_.implementation_id = "native.suggestion_orchestrator";
        descriptor_.implementation_version = "1.0.0";
        descriptor_.kind = "cognition";
        descriptor_.provides.push_back(
            {"evaluate.suggestion_decision", "contracts/schemas/suggestion_decision.schema.json"});
        descriptor_.supports_hot_plug = true;
        descriptor_.supports_checkpoint = false;
    }

    const CapabilityDescriptor& descriptor() const override { return descriptor_; }
    void validate_manifest() override {}
    void configure() override {}
    void initialize() override {}
    void calibrate() override {}
    bool health_check() override { return true; }
    void start() override {}
    void drain() override {}
    std::map<std::string, std::string> checkpoint() override { return {}; }
    void stop() override {}
    void uninstall() override {}

private:
    CapabilityDescriptor descriptor_;
};

}  // namespace eu_digital
