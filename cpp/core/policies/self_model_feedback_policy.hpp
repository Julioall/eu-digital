#pragma once

#include "core/functional_self_model.hpp"
#include "core/event_bus.hpp"

#include <cmath>
#include <optional>
#include <string>

// Simple JSON parser for the CanonicalEvent payload since we don't have a full JSON parser yet.
// In a real implementation this would use a robust JSON library.
namespace eu_digital {

inline std::string extract_json_field(const std::string& json, const std::string& field) {
    auto pos = json.find("\"" + field + "\":");
    if (pos == std::string::npos) return "";
    pos += field.length() + 3;
    if (json[pos] == '"') {
        auto end = json.find("\"", pos + 1);
        if (end == std::string::npos) return "";
        return json.substr(pos + 1, end - pos - 1);
    } else {
        auto end = json.find_first_of(",}", pos);
        if (end == std::string::npos) return "";
        return json.substr(pos, end - pos);
    }
}

class SelfModelFeedbackPolicy {
public:
    explicit SelfModelFeedbackPolicy(VersionedFunctionalSelfModel& self_model) : self_model_(self_model) {}

    void evaluate(const CanonicalEvent& event, const std::string& occurred_at) {
        if (event.event_type != "action_outcome") return;
        
        std::string status = extract_json_field(event.payload, "status");
        std::string operation = extract_json_field(event.payload, "operation"); // We might need to map plan_id to operation, but let's assume it's there or we use plan_id/target as capability_id context for now. If operation is missing, we use "actuation.supervised".
        if (operation.empty()) operation = "actuation.supervised";

        double confidence_delta = 0.0;
        if (status == "succeeded") {
            confidence_delta = 0.1;
        } else if (status == "failed" || status == "outcome_unknown") {
            confidence_delta = -0.2;
        } else {
            return; // blocked, rolled_back, rollback_failed might not affect confidence directly or maybe rollback_failed does.
        }

        // Get current capability to decay/improve.
        const auto& current_snapshot = self_model_.current();
        const auto& capabilities = current_snapshot.capabilities;
        FunctionalSelfModelCapability current_cap;
        current_cap.capability_id = operation;
        current_cap.status = "available";
        current_cap.explanation = "Derived from action feedback";
        current_cap.confidence_score = 1.0;

        auto it = std::find_if(capabilities.begin(), capabilities.end(), [&](const auto& cap) {
            return cap.capability_id == operation;
        });
        if (it != capabilities.end()) {
            current_cap = *it;
        }

        current_cap.confidence_score += confidence_delta;
        if (current_cap.confidence_score > 1.0) current_cap.confidence_score = 1.0;
        if (current_cap.confidence_score < 0.0) current_cap.confidence_score = 0.0;

        if (current_cap.confidence_score < 0.2) {
            current_cap.status = "unavailable";
            current_cap.explanation = "Capability is unavailable due to critically low confidence from repeated failures.";
        } else if (current_cap.confidence_score < 0.5) {
            current_cap.status = "degraded";
            current_cap.explanation = "Capability is degraded due to recent failures.";
        } else {
            current_cap.status = "available";
            current_cap.explanation = "Capability is operating normally.";
        }

        current_cap.source_event_ids.push_back(event.event_id);

        FunctionalSelfModelEvent update_event;
        update_event.event_id = "fb-" + event.event_id;
        update_event.occurred_at = occurred_at;
        update_event.kind = "capability_changed";
        update_event.reason = "action_feedback";
        update_event.source_event_ids = {event.event_id};
        update_event.capability = current_cap;

        self_model_.apply(update_event);
    }

private:
    VersionedFunctionalSelfModel& self_model_;
};

} // namespace eu_digital
