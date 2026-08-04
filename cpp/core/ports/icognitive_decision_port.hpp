#pragma once

#include "core/event_bus.hpp"
#include "core/contracts/cognitive_port_requests.hpp"
#include "core/contracts/cognitive_cycle_v1.hpp"
#include "core/contracts/cognitive_decision.hpp"
#include "core/contracts/cognitive_cycle_context.hpp"
#include "core/contracts/port_result.hpp"

#include <stdexcept>

namespace eu_digital {

class ICognitiveDecisionPort {
public:
    virtual ~ICognitiveDecisionPort() = default;

    virtual CognitiveDecision decide(const CanonicalEvent& event, const CognitiveCycleContext& ctx) = 0;

    virtual CognitiveDecision decide_evidence(
        const contracts::DecisionRequest&) {
        throw std::logic_error("decision requests are not implemented");
    }

    contracts::PortResult<CognitiveDecision> decide_result(
        const CanonicalEvent& event, const CognitiveCycleContext& context) {
        return contracts::capture_port_result<CognitiveDecision>(
            "decision.decide", [&] { return decide(event, context); });
    }

    contracts::PortResult<CognitiveDecision> decide_evidence_result(
        const contracts::DecisionRequest& request) {
        return contracts::capture_port_result<CognitiveDecision>(
            "decision.decide_evidence", [&] { return decide_evidence(request); });
    }

    virtual contracts::PortResult<CognitiveDecision> decide_evidence_context(
        const contracts::DecisionRequest& request,
        const contracts::PortInvocationContextV1& context) {
        if (context.stop_requested()) {
            return contracts::PortResult<CognitiveDecision>::failed(
                "decision.decide_evidence", "cancelled",
                "cycle invocation was cancelled");
        }
        return decide_evidence_result(request);
    }
};

} // namespace eu_digital
