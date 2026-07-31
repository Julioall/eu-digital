#pragma once

#include "core/event_bus.hpp"
#include "core/contracts/cognitive_decision.hpp"

namespace eu_digital {

class ICognitiveDecisionPort {
public:
    virtual ~ICognitiveDecisionPort() = default;

    virtual CognitiveDecision decide(const CanonicalEvent& event) = 0;
};

} // namespace eu_digital
