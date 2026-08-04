#pragma once

#include "core/metacognition_curiosity.hpp"
#include "core/ports/imetacognition_port.hpp"

#include <algorithm>
#include <memory>
#include <mutex>
#include <stdexcept>

namespace eu_digital {

class MetacognitionCuriosityAdapter final : public IMetacognitionPort {
public:
    explicit MetacognitionCuriosityAdapter(
        std::shared_ptr<MetacognitionCuriosityEngine> engine)
        : engine_(std::move(engine)) {
        if (!engine_) throw std::invalid_argument("engine cannot be null");
    }

    contracts::MetacognitiveAssessment evaluate(
        const contracts::WorkspaceSnapshot&) override {
        throw std::invalid_argument(
            "legacy workspace snapshot cannot represent a hypothesis");
    }

    contracts::MetacognitivePortAssessment evaluate_hypothesis(
        const contracts::MetacognitionRequest& request) override {
        if (!request.valid()) {
            throw std::invalid_argument("invalid metacognition request");
        }
        std::lock_guard lock(mutex_);

        HypothesisRecord hypothesis;
        hypothesis.hypothesis_id = request.hypothesis.hypothesis_id;
        hypothesis.kind = request.hypothesis.kind;
        hypothesis.statement = request.hypothesis.statement;
        hypothesis.status = status_from_string(request.hypothesis.status);
        hypothesis.confidence = request.hypothesis.confidence;
        hypothesis.supporting_refs = request.hypothesis.supporting_refs;
        if (request.workspace_snapshot_id &&
            std::find(hypothesis.supporting_refs.begin(),
                      hypothesis.supporting_refs.end(),
                      *request.workspace_snapshot_id) ==
                hypothesis.supporting_refs.end()) {
            hypothesis.supporting_refs.push_back(*request.workspace_snapshot_id);
        }
        hypothesis.opposing_refs = request.hypothesis.opposing_refs;
        hypothesis.alternatives = request.hypothesis.alternatives;
        hypothesis.created_at = request.hypothesis.created_at;
        hypothesis.updated_at = request.hypothesis.updated_at;
        hypothesis.verification_question = request.hypothesis.verification_question;
        hypothesis.expected_information_gain =
            request.hypothesis.expected_information_gain;
        hypothesis.provenance_module = request.hypothesis.provenance_module;
        hypothesis.model_version = request.hypothesis.model_version;
        hypothesis.schema_version = request.hypothesis.schema_version;
        const auto assessment = engine_->evaluate(hypothesis, request.evaluated_at);

        contracts::MetacognitivePortAssessment result;
        result.assessment_id = assessment.assessment_id;
        result.hypothesis_id = assessment.hypothesis_id;
        result.evaluated_at = assessment.evaluated_at;
        result.curiosity_score = assessment.uncertainty;
        result.requires_exploration = assessment.decision == "question";
        result.focus_area = assessment.hypothesis_id;
        return result;
    }

private:
    static HypothesisStatus status_from_string(const std::string& status) {
        if (status == "proposed") return HypothesisStatus::proposed;
        if (status == "confirmed") return HypothesisStatus::confirmed;
        if (status == "rejected") return HypothesisStatus::rejected;
        if (status == "superseded") return HypothesisStatus::superseded;
        throw std::invalid_argument("unsupported hypothesis status");
    }

    std::shared_ptr<MetacognitionCuriosityEngine> engine_;
    std::mutex mutex_;
};

}  // namespace eu_digital
