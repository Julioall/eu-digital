#pragma once

#include "core/pattern_learner.hpp"
#include "core/ports/ipattern_learning_port.hpp"

#include <memory>
#include <mutex>
#include <stdexcept>
#include <utility>
#include <vector>

namespace eu_digital {

class PatternLearnerAdapter final : public IPatternLearningPort {
public:
    explicit PatternLearnerAdapter(std::shared_ptr<PatternLearner> learner)
        : learner_(std::move(learner)) {
        if (!learner_) {
            throw std::invalid_argument("learner cannot be null");
        }
    }

    contracts::PatternLearningResult observe(
        const contracts::PatternLearningObservation& observation) override {
        std::lock_guard lock(mutex_);
        try {
            PatternObservation native_observation;
            native_observation.features = observation.features;
            native_observation.observation_ref = observation.observation_ref;
            native_observation.occurred_epoch = observation.occurred_epoch;
            return contracts::PatternLearningResult::ok(
                to_contract(learner_->observe(native_observation)));
        } catch (const std::exception& error) {
            return contracts::PatternLearningResult::failed(
                "pattern_learning.observe", "adapter_delegation_error", error.what());
        }
    }

    contracts::PatternLearningResult feedback(
        const contracts::PatternLearningFeedback& feedback) override {
        std::lock_guard lock(mutex_);
        try {
            return contracts::PatternLearningResult::ok(
                to_contract(learner_->feedback(
                    feedback.pattern_id, feedback.positive, feedback.reference)));
        } catch (const std::exception& error) {
            return contracts::PatternLearningResult::failed(
                "pattern_learning.feedback", "adapter_delegation_error", error.what());
        }
    }

    std::vector<contracts::ObservedPattern> snapshot() const override {
        std::lock_guard lock(mutex_);
        std::vector<contracts::ObservedPattern> result;
        for (const auto& record : learner_->records()) {
            result.push_back(to_contract(record));
        }
        return result;
    }

private:
    static contracts::ObservedPattern to_contract(const PatternRecord& record) {
        contracts::ObservedPattern result;
        result.pattern_id = record.pattern_id;
        result.schema_version = record.schema_version;
        result.version = record.version;
        result.status = record.status;
        result.centroid = record.centroid;
        result.support = record.support;
        result.stability = record.stability;
        result.recency = record.recency;
        result.confidence = record.confidence;
        result.observation_refs = record.observation_refs;
        result.positive_feedback = record.positive_feedback;
        result.negative_feedback = record.negative_feedback;
        result.feedback_references = record.feedback_references;
        result.parent_pattern_id = record.parent_pattern_id;
        result.drift_reason = record.drift_reason;
        result.created_by = record.created_by;
        return result;
    }

    std::shared_ptr<PatternLearner> learner_;
    mutable std::mutex mutex_;
};

}  // namespace eu_digital
