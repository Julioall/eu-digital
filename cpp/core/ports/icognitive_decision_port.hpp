#pragma once

#include "core/event_bus.hpp"
#include "core/contracts/cognitive_decision.hpp"
#include "core/contracts/cognitive_cycle_context.hpp"
#include "core/contracts/port_result.hpp"

namespace eu_digital {

class ICognitiveDecisionPort {
public:
    virtual ~ICognitiveDecisionPort() = default;

    virtual CognitiveDecision decide(const CanonicalEvent& event, const CognitiveCycleContext& ctx) = 0;

    contracts::PortResult<CognitiveDecision> decide_result(
        const CanonicalEvent& event, const CognitiveCycleContext& context) {
        return contracts::capture_port_result<CognitiveDecision>(
            "decision.decide", [&] { return decide(event, context); });
    }
};

} // namespace eu_digital
