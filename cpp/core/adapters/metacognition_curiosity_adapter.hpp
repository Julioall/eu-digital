#pragma once

#include "core/ports/imetacognition_port.hpp"
#include "core/metacognition_curiosity.hpp"
#include <memory>
#include <mutex>
#include <stdexcept>

namespace eu_digital {

class MetacognitionCuriosityAdapter final : public IMetacognitionPort {
public:
    explicit MetacognitionCuriosityAdapter(std::shared_ptr<MetacognitionCuriosityEngine> engine)
        : engine_(std::move(engine)) {
        if (!engine_) {
            throw std::invalid_argument("engine cannot be null");
        }
    }

    contracts::MetacognitiveAssessment evaluate(const contracts::WorkspaceSnapshot& workspace) override {
        std::lock_guard lock(mutex_);
        
        HypothesisRecord dummy;
        dummy.hypothesis_id = "hyp-adapter";
        dummy.kind = "causal";
        dummy.statement = "abstracted hypothesis from workspace";
        dummy.confidence = 0.5;
        dummy.supporting_refs = {"ref1"};
        dummy.opposing_refs = {"ref2"};
        dummy.alternatives = {"alt1"};
        dummy.created_at = "2026-07-31T12:00:00Z";
        dummy.updated_at = "2026-07-31T12:00:00Z";
        dummy.provenance_module = "adapter";
        
        auto internal_assessment = engine_->evaluate(dummy, "2026-07-31T12:00:00Z");
        
        contracts::MetacognitiveAssessment result;
        result.assessment_id = internal_assessment.assessment_id;
        result.curiosity_score = internal_assessment.uncertainty;
        result.requires_exploration = (internal_assessment.decision == "question");
        result.focus_area = internal_assessment.hypothesis_id;
        return result;
    }

private:
    std::shared_ptr<MetacognitionCuriosityEngine> engine_;
    std::mutex mutex_;
};

} // namespace eu_digital
