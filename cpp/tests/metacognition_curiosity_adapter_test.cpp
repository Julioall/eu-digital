#include "core/adapters/metacognition_curiosity_adapter.hpp"
#include <cassert>
#include <iostream>

using namespace eu_digital;

int main() {
    try {
        auto engine = std::make_shared<MetacognitionCuriosityEngine>();
        MetacognitionCuriosityAdapter adapter(engine);
        
        contracts::MetacognitionRequest request;
        request.hypothesis.hypothesis_id = "hypothesis-1";
        request.hypothesis.kind = "predictive";
        request.hypothesis.statement = "The editor remains active";
        request.hypothesis.status = "proposed";
        request.hypothesis.confidence = 0.7;
        request.hypothesis.supporting_refs = {"event-1"};
        request.hypothesis.created_at = "2026-08-04T12:00:00Z";
        request.hypothesis.updated_at = "2026-08-04T12:00:00Z";
        request.hypothesis.provenance_module = "adapter-test";
        request.evaluated_at = "2026-08-04T12:00:01Z";
        request.workspace_snapshot_id = "snapshot-1";
        
        auto result = adapter.evaluate_hypothesis_result(request);
        assert(result.valid());
        assert(result.success);
        assert(result.value);
        assert(result.value->valid());
        assert(result.value->hypothesis_id == "hypothesis-1");
        assert(result.value->evaluated_at == "2026-08-04T12:00:01+00:00");
        assert(result.value->focus_area == request.hypothesis.hypothesis_id);
        assert(engine->snapshot_json().find("snapshot-1") != std::string::npos);

        auto invalid = request;
        invalid.hypothesis.hypothesis_id.clear();
        assert(!adapter.evaluate_hypothesis_result(invalid).success);

        contracts::WorkspaceSnapshot legacy;
        legacy.workspace_id = "ws1";
        assert(!adapter.evaluate_result(legacy).success);
        
        std::cout << "MetacognitionCuriosityAdapter tests passed!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed: " << e.what() << std::endl;
        return 1;
    }
}
