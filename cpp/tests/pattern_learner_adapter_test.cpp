#include "core/adapters/pattern_learner_adapter.hpp"

#include <cassert>
#include <memory>
#include <string>

using eu_digital::PatternConfig;
using eu_digital::PatternLearner;
using eu_digital::PatternLearnerAdapter;
using eu_digital::contracts::PatternLearningFeedback;
using eu_digital::contracts::PatternLearningObservation;

namespace {

PatternLearningObservation observation(double value, std::string reference, double timestamp) {
    PatternLearningObservation result;
    result.features = {{"application.vscode", value}};
    result.observation_ref = std::move(reference);
    result.occurred_epoch = timestamp;
    return result;
}

}  // namespace

int main() {
    auto learner = std::make_shared<PatternLearner>(PatternConfig{2, 0.1, 0.5}, "adapter-stream");
    PatternLearnerAdapter adapter(learner);

    const auto first = adapter.observe(observation(1.0, "event-1", 1.0));
    assert(first.success);
    assert(first.value);
    assert(first.value->status == "candidate");
    assert(first.value->observation_refs == std::vector<std::string>{"event-1"});

    const auto second = adapter.observe(observation(1.0, "event-2", 2.0));
    assert(second.success);
    assert(second.value);
    assert(second.value->pattern_id == first.value->pattern_id);
    assert(second.value->status == "promoted");
    assert(second.value->support == 2);
    assert(second.value->observation_refs.size() == 2);

    PatternLearningFeedback feedback;
    feedback.pattern_id = second.value->pattern_id;
    feedback.positive = false;
    feedback.reference = "human-correction-1";
    const auto corrected = adapter.feedback(feedback);
    assert(corrected.success);
    assert(corrected.value);
    assert(corrected.value->status == "candidate");
    assert(corrected.value->negative_feedback == 1);
    assert(corrected.value->feedback_references ==
           std::vector<std::string>{"human-correction-1"});

    PatternLearningObservation invalid;
    invalid.observation_ref = "event-invalid";
    invalid.occurred_epoch = 3.0;
    const auto failed = adapter.observe(invalid);
    assert(!failed.success);
    assert(!failed.value);
    assert(failed.error);
    assert(failed.error->operation == "pattern_learning.observe");
    assert(failed.error->code == "adapter_delegation_error");
    assert(!failed.error->message.empty());

    const auto snapshot = adapter.snapshot();
    assert(snapshot.size() == 1);
    assert(snapshot.front().pattern_id == first.value->pattern_id);

    const auto snapshot_result = adapter.snapshot_result();
    assert(snapshot_result.valid());
    assert(snapshot_result.value);
    assert(snapshot_result.value->size() == 1);

    bool null_rejected = false;
    try {
        PatternLearnerAdapter invalid_adapter(nullptr);
    } catch (const std::invalid_argument&) {
        null_rejected = true;
    }
    assert(null_rejected);
}
