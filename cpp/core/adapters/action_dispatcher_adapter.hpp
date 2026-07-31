#pragma once

#include "core/supervised_actions.hpp"
#include "core/contracts/cognitive_decision.hpp"
#include "core/digest.hpp"

#include <unordered_set>
#include <string>
#include <mutex>
#include <optional>

namespace eu_digital {

class ActionDispatcherAdapter {
public:
    explicit ActionDispatcherAdapter(SupervisedActionPlugin& supervised_action_plugin)
        : plugin_(supervised_action_plugin) {}

    std::optional<ActionPreparation> dispatch(const CognitiveDecision& decision, std::uint64_t now_ms) {
        if (!decision.success || decision.intent != "action") {
            return std::nullopt; // Only handle successful action decisions
        }
        
        // We need a stable plan digest. The cognitive decision has a "target_action" payload (JSON).
        std::string payload = decision.target_action.empty() ? "{}" : decision.target_action;
        std::string plan_digest = digest::hex(digest::sha256(payload));

        {
            std::lock_guard lock(mutex_);
            if (dispatched_digests_.contains(plan_digest)) {
                // At-most-once semantics: already dispatched this plan.
                return std::nullopt; 
            }
            dispatched_digests_.insert(plan_digest);
        }

        ActionPlan plan;
        plan.plan_id = "plan-" + digest::uuid5("c70b62a7-d37b-4ee9-9a58-3d595147e353", plan_digest);
        plan.plan_digest = plan_digest;
        
        // Extract basic operation from target_action or use default
        std::string operation = "unknown_operation";
        auto pos = payload.find("\"operation\":");
        if (pos != std::string::npos) {
            pos += 12;
            if (payload[pos] == '"') {
                auto end = payload.find("\"", pos + 1);
                if (end != std::string::npos) {
                    operation = payload.substr(pos + 1, end - pos - 1);
                }
            }
        }
        plan.operation = operation;
        plan.target = payload;
        plan.requested_effects = {"Cognitive intention execution"};

        return plugin_.prepare(plan, now_ms);
    }

private:
    SupervisedActionPlugin& plugin_;
    std::mutex mutex_;
    std::unordered_set<std::string> dispatched_digests_;
};

} // namespace eu_digital
