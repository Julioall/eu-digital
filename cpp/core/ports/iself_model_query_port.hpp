#pragma once

#include "core/contracts/self_constraint_snapshot.hpp"

namespace eu_digital {

class ISelfModelQueryPort {
public:
    virtual ~ISelfModelQueryPort() = default;

    virtual SelfConstraintSnapshot query_constraints() = 0;
};

} // namespace eu_digital
