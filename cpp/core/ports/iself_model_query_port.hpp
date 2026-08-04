#pragma once

#include "core/contracts/port_result.hpp"
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
};

} // namespace eu_digital
