#pragma once

#include "core/capability_runtime.hpp"
#include "core/digest.hpp"

#include <algorithm>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace eu_digital {

inline constexpr const char* FUNCTIONAL_SELF_MODEL_SCHEMA_VERSION = "1.0";
inline constexpr const char* FUNCTIONAL_SELF_MODEL_POLICY_ID = "self_model_gate_v1";
inline constexpr const char* FUNCTIONAL_SELF_MODEL_BASELINE_ID = "unconstrained_decision_v0";
inline constexpr const char* FUNCTIONAL_SELF_MODEL_NAMESPACE = "c70b62a7-d37b-4ee9-9a58-3d595147e353";
inline constexpr const char* FUNCTIONAL_SELF_MODEL_HYPOTHESIS =
    "a versioned self-model gate improves capability attribution, limitation explanations, and compatible decision selection versus an unconstrained control";
inline constexpr const char* FUNCTIONAL_SELF_MODEL_ABLATION =
    "select unconstrained_decision_v0 through the same interface and omit snapshot consultation during a decision";
inline constexpr const char* FUNCTIONAL_SELF_MODEL_FALSIFICATION =
    "removing snapshot consultation does not change relevant decisions, or snapshots misrepresent declared capability availability";

inline std::string functional_self_model_escape_json(const std::string& value) {
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

inline std::string functional_self_model_json_string(const std::string& value) {
    return "\"" + functional_self_model_escape_json(value) + "\"";
}

inline std::string functional_self_model_json_array(const std::vector<std::string>& values) {
    std::ostringstream output;
    output << '[';
    for (std::size_t index = 0; index < values.size(); ++index) {
        if (index) output << ',';
        output << functional_self_model_json_string(values[index]);
    }
    output << ']';
    return output.str();
}

struct FunctionalSelfModelAssertion {
    std::string assertion_id;
    std::string subject;
    std::string predicate;
    std::string value;
    std::string classification;
    std::string explanation;
    std::vector<std::string> source_event_ids;

    void validate() const {
        if (assertion_id.empty() || subject.empty() || predicate.empty() || value.empty() || explanation.empty()) {
            throw std::invalid_argument("self-model assertion fields are required");
        }
        if (classification != "fact" && classification != "hypothesis" && classification != "configuration") {
            throw std::invalid_argument("unsupported self-model assertion classification");
        }
        if (std::any_of(source_event_ids.begin(), source_event_ids.end(), [](const auto& item) { return item.empty(); })) {
            throw std::invalid_argument("self-model assertion source references must be non-empty");
        }
    }
};

struct FunctionalSelfModelCapability {
    std::string capability_id;
    std::string status;
    std::string explanation;
    std::vector<std::string> source_event_ids;

    void validate() const {
        if (capability_id.empty() || explanation.empty()) throw std::invalid_argument("self-model capability fields are required");
        if (status != "available" && status != "degraded" && status != "unavailable" && status != "removed") {
            throw std::invalid_argument("unsupported self-model capability status");
        }
        if (std::any_of(source_event_ids.begin(), source_event_ids.end(), [](const auto& item) { return item.empty(); })) {
            throw std::invalid_argument("self-model capability source references must be non-empty");
        }
    }
};

struct FunctionalSelfModelEvent {
    std::string event_id;
    std::string schema_version{FUNCTIONAL_SELF_MODEL_SCHEMA_VERSION};
    std::string occurred_at;
    std::string kind;
    std::string reason;
    std::vector<std::string> source_event_ids;
    std::optional<FunctionalSelfModelCapability> capability;
    std::optional<FunctionalSelfModelAssertion> assertion;

    void validate() const {
        if (event_id.empty() || occurred_at.empty() || reason.empty()) {
            throw std::invalid_argument("self-model event fields are required");
        }
        if (schema_version != FUNCTIONAL_SELF_MODEL_SCHEMA_VERSION) {
            throw std::invalid_argument("unsupported self-model event schema version");
        }
        if (kind != "capability_changed" && kind != "assertion_recorded") {
            throw std::invalid_argument("unsupported self-model event kind");
        }
        if (std::any_of(source_event_ids.begin(), source_event_ids.end(), [](const auto& item) { return item.empty(); })) {
            throw std::invalid_argument("self-model event source references must be non-empty");
        }
        if (kind == "capability_changed") {
            if (!capability || assertion) throw std::invalid_argument("capability event payload is invalid");
            capability->validate();
        } else {
            if (!assertion || capability) throw std::invalid_argument("assertion event payload is invalid");
            assertion->validate();
        }
    }
};

struct FunctionalSelfModelSnapshot {
    std::string snapshot_id;
    std::string schema_version{FUNCTIONAL_SELF_MODEL_SCHEMA_VERSION};
    int version{0};
    std::optional<std::string> prior_snapshot_id;
    std::string updated_at;
    std::optional<std::string> trigger_event_id;
    std::vector<FunctionalSelfModelCapability> capabilities;
    std::vector<FunctionalSelfModelAssertion> facts;
    std::vector<FunctionalSelfModelAssertion> hypotheses;
    std::vector<FunctionalSelfModelAssertion> configuration;
    std::string history_hash;
};

struct FunctionalSelfModelDecision {
    std::string decision_id;
    std::string schema_version{FUNCTIONAL_SELF_MODEL_SCHEMA_VERSION};
    std::string snapshot_id;
    std::string requested_capability_id;
    bool allowed{false};
    std::string reason_code;
    std::string explanation;
    std::string policy_id;
};

class VersionedFunctionalSelfModel {
public:
    VersionedFunctionalSelfModel(std::string model_id, std::string initial_at,
                                 std::string decision_policy = FUNCTIONAL_SELF_MODEL_POLICY_ID)
        : model_id_(std::move(model_id)), decision_policy_(std::move(decision_policy)) {
        if (model_id_.empty() || initial_at.empty()) throw std::invalid_argument("self-model names and initial time are required");
        if (decision_policy_ != FUNCTIONAL_SELF_MODEL_POLICY_ID && decision_policy_ != FUNCTIONAL_SELF_MODEL_BASELINE_ID) {
            throw std::invalid_argument("unsupported self-model decision policy");
        }
        history_.push_back(make_snapshot(0, nullptr, initial_at, std::nullopt, {}, {}, {}, {}));
    }

    const FunctionalSelfModelSnapshot& current() const { return history_.back(); }
    const std::vector<FunctionalSelfModelSnapshot>& history() const { return history_; }
    const std::string& model_id() const { return model_id_; }
    const std::string& decision_policy() const { return decision_policy_; }
    std::size_t applied_event_count() const { return seen_event_ids_.size(); }
    std::size_t history_version_count() const { return history_.size(); }

    const FunctionalSelfModelSnapshot& version(int version) const {
        if (version < 0 || static_cast<std::size_t>(version) >= history_.size()) {
            throw std::invalid_argument("requested self-model version is unavailable");
        }
        return history_.at(static_cast<std::size_t>(version));
    }

    FunctionalSelfModelSnapshot apply(FunctionalSelfModelEvent event) {
        event.validate();
        if (seen_event_ids_.contains(event.event_id)) throw std::invalid_argument("internal event was already applied");
        if (event.occurred_at < current().updated_at) {
            throw std::invalid_argument("internal events must be applied in timestamp order");
        }
        auto capabilities = capabilities_;
        auto facts = facts_;
        auto hypotheses = hypotheses_;
        auto configuration = configuration_;
        if (event.kind == "capability_changed") {
            capabilities[event.capability->capability_id] = *event.capability;
        } else {
            auto& target = event.assertion->classification == "fact"
                ? facts : (event.assertion->classification == "hypothesis" ? hypotheses : configuration);
            target.push_back(*event.assertion);
        }
        std::vector<FunctionalSelfModelCapability> ordered_capabilities;
        for (const auto& [unused, entry] : capabilities) ordered_capabilities.push_back(entry);
        sort_capabilities(ordered_capabilities);
        sort_assertions(facts);
        sort_assertions(hypotheses);
        sort_assertions(configuration);
        auto next = make_snapshot(current().version + 1, &current(), event.occurred_at, event.event_id,
                                  std::move(ordered_capabilities), std::move(facts),
                                  std::move(hypotheses), std::move(configuration));
        seen_event_ids_.insert(event.event_id);
        capabilities_.clear();
        for (const auto& entry : next.capabilities) capabilities_[entry.capability_id] = entry;
        facts_ = next.facts;
        hypotheses_ = next.hypotheses;
        configuration_ = next.configuration;
        history_.push_back(next);
        return next;
    }

    FunctionalSelfModelDecision decide(const std::string& requested_capability_id) const {
        if (requested_capability_id.empty()) throw std::invalid_argument("requested capability id is required");
        bool allowed = false;
        std::string reason_code;
        std::string explanation;
        if (decision_policy_ == FUNCTIONAL_SELF_MODEL_BASELINE_ID) {
            allowed = true;
            reason_code = "baseline_unconstrained";
            explanation = "Baseline does not consult the functional self-model.";
        } else {
            const auto found = capabilities_.find(requested_capability_id);
            if (found == capabilities_.end()) {
                reason_code = "capability_unverified";
                explanation = "Capability " + requested_capability_id + " is not declared; availability is unverified.";
            } else {
                const auto& entry = found->second;
                explanation = entry.explanation;
                if (entry.status == "available") {
                    allowed = true;
                    reason_code = "capability_available";
                } else if (entry.status == "degraded") {
                    reason_code = "capability_degraded";
                } else if (entry.status == "unavailable") {
                    reason_code = "capability_unavailable";
                } else {
                    reason_code = "capability_removed";
                }
            }
        }
        const auto decision_id = digest::uuid5(
            FUNCTIONAL_SELF_MODEL_NAMESPACE,
            current().snapshot_id + ":" + requested_capability_id + ":" + decision_policy_ + ":" + reason_code);
        return {decision_id, FUNCTIONAL_SELF_MODEL_SCHEMA_VERSION, current().snapshot_id,
                requested_capability_id, allowed, reason_code, explanation, decision_policy_};
    }

    std::map<std::string, std::string> metrics() const {
        return {
            {"ablation", FUNCTIONAL_SELF_MODEL_ABLATION},
            {"applied_event_count", std::to_string(seen_event_ids_.size())},
            {"baseline_policy_id", FUNCTIONAL_SELF_MODEL_BASELINE_ID},
            {"falsification", FUNCTIONAL_SELF_MODEL_FALSIFICATION},
            {"history_version_count", std::to_string(history_.size())},
            {"hypothesis", FUNCTIONAL_SELF_MODEL_HYPOTHESIS},
            {"policy_id", decision_policy_},
        };
    }

private:
    static void sort_capabilities(std::vector<FunctionalSelfModelCapability>& values) {
        std::sort(values.begin(), values.end(), [](const auto& left, const auto& right) {
            return left.capability_id < right.capability_id;
        });
    }

    static void sort_assertions(std::vector<FunctionalSelfModelAssertion>& values) {
        std::sort(values.begin(), values.end(), [](const auto& left, const auto& right) {
            return left.assertion_id < right.assertion_id;
        });
    }

    static std::string assertion_json(const FunctionalSelfModelAssertion& value) {
        std::ostringstream output;
        output << "{\"assertion_id\":" << functional_self_model_json_string(value.assertion_id)
               << ",\"classification\":" << functional_self_model_json_string(value.classification)
               << ",\"explanation\":" << functional_self_model_json_string(value.explanation)
               << ",\"predicate\":" << functional_self_model_json_string(value.predicate)
               << ",\"source_event_ids\":" << functional_self_model_json_array(value.source_event_ids)
               << ",\"subject\":" << functional_self_model_json_string(value.subject)
               << ",\"value\":" << functional_self_model_json_string(value.value) << '}';
        return output.str();
    }

    static std::string capability_json(const FunctionalSelfModelCapability& value) {
        std::ostringstream output;
        output << "{\"capability_id\":" << functional_self_model_json_string(value.capability_id)
               << ",\"explanation\":" << functional_self_model_json_string(value.explanation)
               << ",\"source_event_ids\":" << functional_self_model_json_array(value.source_event_ids)
               << ",\"status\":" << functional_self_model_json_string(value.status) << '}';
        return output.str();
    }

    static std::string object_array(const std::vector<FunctionalSelfModelCapability>& values) {
        std::ostringstream output;
        output << '[';
        for (std::size_t index = 0; index < values.size(); ++index) {
            if (index) output << ',';
            output << capability_json(values[index]);
        }
        output << ']';
        return output.str();
    }

    static std::string assertion_array(const std::vector<FunctionalSelfModelAssertion>& values) {
        std::ostringstream output;
        output << '[';
        for (std::size_t index = 0; index < values.size(); ++index) {
            if (index) output << ',';
            output << assertion_json(values[index]);
        }
        output << ']';
        return output.str();
    }

    FunctionalSelfModelSnapshot make_snapshot(
        int version, const FunctionalSelfModelSnapshot* prior, const std::string& updated_at,
        std::optional<std::string> trigger_event_id,
        std::vector<FunctionalSelfModelCapability> capabilities,
        std::vector<FunctionalSelfModelAssertion> facts,
        std::vector<FunctionalSelfModelAssertion> hypotheses,
        std::vector<FunctionalSelfModelAssertion> configuration) const {
        sort_capabilities(capabilities);
        sort_assertions(facts);
        sort_assertions(hypotheses);
        sort_assertions(configuration);
        const auto prior_snapshot_id = prior == nullptr ? std::optional<std::string>{} : std::optional<std::string>{prior->snapshot_id};
        const auto prior_history_hash = prior == nullptr ? std::optional<std::string>{} : std::optional<std::string>{prior->history_hash};
        std::ostringstream payload;
        payload << "{\"capabilities\":" << object_array(capabilities)
                << ",\"configuration\":" << assertion_array(configuration)
                << ",\"facts\":" << assertion_array(facts)
                << ",\"hypotheses\":" << assertion_array(hypotheses)
                << ",\"model_id\":" << functional_self_model_json_string(model_id_)
                << ",\"prior_history_hash\":"
                << (prior_history_hash ? functional_self_model_json_string(*prior_history_hash) : "null")
                << ",\"prior_snapshot_id\":"
                << (prior_snapshot_id ? functional_self_model_json_string(*prior_snapshot_id) : "null")
                << ",\"trigger_event_id\":"
                << (trigger_event_id ? functional_self_model_json_string(*trigger_event_id) : "null")
                << ",\"updated_at\":" << functional_self_model_json_string(updated_at)
                << ",\"version\":" << version << '}';
        const auto history_hash = digest::hex(digest::sha256(payload.str()));
        return {
            digest::uuid5(FUNCTIONAL_SELF_MODEL_NAMESPACE, model_id_ + ":" + history_hash),
            FUNCTIONAL_SELF_MODEL_SCHEMA_VERSION,
            version,
            prior_snapshot_id,
            updated_at,
            std::move(trigger_event_id),
            std::move(capabilities),
            std::move(facts),
            std::move(hypotheses),
            std::move(configuration),
            history_hash,
        };
    }

    std::string model_id_;
    std::string decision_policy_;
    std::set<std::string> seen_event_ids_;
    std::map<std::string, FunctionalSelfModelCapability> capabilities_;
    std::vector<FunctionalSelfModelAssertion> facts_;
    std::vector<FunctionalSelfModelAssertion> hypotheses_;
    std::vector<FunctionalSelfModelAssertion> configuration_;
    std::vector<FunctionalSelfModelSnapshot> history_;
};

class FunctionalSelfModelPlugin final : public CapabilityPlugin {
public:
    FunctionalSelfModelPlugin() {
        descriptor_.capability_id = "cognition.functional_self_model";
        descriptor_.implementation_id = "native.functional_self_model";
        descriptor_.implementation_version = "1.0.0";
        descriptor_.kind = "cognitive_service";
        descriptor_.provides.push_back({"decide.self_model", "urn:eu-digital:self-model-decision:1"});
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
