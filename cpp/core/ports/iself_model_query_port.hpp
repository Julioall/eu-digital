#pragma once

#include "core/contracts/port_result.hpp"
#include "core/contracts/cognitive_cycle_v1.hpp"
#include "core/contracts/self_constraint_snapshot.hpp"

namespace eu_digital {

class ISelfModelQueryPort {
public:
    virtual ~ISelfModelQueryPort() = default;

    virtual SelfConstraintSnapshot query_constraints() = 0;

    contracts::PortResult<SelfConstraintSnapshot> query_constraints_result() {
        return contracts::capture_port_result<SelfConstraintSnapshot>(
            "self_model.query_constraints", [&] { return query_constraints(); });
    }

    virtual contracts::PortResult<SelfConstraintSnapshot> query_constraints_context(
        const contracts::PortInvocationContextV1& context) {
        if (context.stop_requested()) {
            return contracts::PortResult<SelfConstraintSnapshot>::failed(
                "self_model.query_constraints", "cancelled",
                "cycle invocation was cancelled");
        }
        return query_constraints_result();
    }
};

} // namespace eu_digital
