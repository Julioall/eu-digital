#pragma once

#include "core/ports/iself_model_query_port.hpp"
#include "core/functional_self_model.hpp"
#include <memory>
#include <mutex>
#include <stdexcept>

namespace eu_digital {

class FunctionalSelfModelAdapter final : public ISelfModelQueryPort {
public:
    explicit FunctionalSelfModelAdapter(std::shared_ptr<VersionedFunctionalSelfModel> self_model)
        : self_model_(std::move(self_model)) {
        if (!self_model_) {
            throw std::invalid_argument("self_model cannot be null");
        }
    }

    SelfConstraintSnapshot query_constraints() override {
        std::lock_guard lock(mutex_);
        // Provide the abstraction over VersionedFunctionalSelfModel
        SelfConstraintSnapshot snapshot;
        snapshot.model_id = self_model_->model_id();
        snapshot.alignment_score = 1.0;
        return snapshot;
    }

private:
    std::shared_ptr<VersionedFunctionalSelfModel> self_model_;
    std::mutex mutex_;
};

} // namespace eu_digital
