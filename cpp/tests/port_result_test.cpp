#include "core/contracts/port_result.hpp"
#include "core/ports/imemory_write_port.hpp"
#include "core/ports/iprediction_port.hpp"

#include <cassert>
#include <stdexcept>

using namespace eu_digital;

namespace {

class SuccessfulMemoryPort final : public IMemoryWritePort {
public:
    MemoryWriteResult store_event(const CanonicalEvent& event) override {
        return MemoryWriteResult::ok(event.event_id);
    }
};

class ThrowingPredictionPort final : public IPredictionPort {
public:
    PredictionAssessment predict(
        const std::vector<std::string>&, const std::string&,
        const std::vector<std::string>& = {}) override {
        throw std::runtime_error("prediction unavailable");
    }

    PredictionAssessment score(
        const PredictionAssessment&, const std::string&, const std::string&) override {
        throw 7;
    }
};

}  // namespace

int main() {
    SuccessfulMemoryPort memory;
    CanonicalEvent event;
    event.event_id = "event-1";

    const auto success = memory.store_event_result(event);
    assert(success.valid());
    assert(success.success);
    assert(success.schema_version == "1.0");
    assert(success.value);
    assert(success.value->memory_id == event.event_id);
    assert(!success.error);

    ThrowingPredictionPort prediction;
    const auto known_failure = prediction.predict_result({}, "now");
    assert(known_failure.valid());
    assert(!known_failure.success);
    assert(!known_failure.value);
    assert(known_failure.error);
    assert(known_failure.error->schema_version == "1.0");
    assert(known_failure.error->operation == "prediction.predict");
    assert(known_failure.error->code == "adapter_delegation_error");
    assert(known_failure.error->message == "prediction unavailable");
    assert(!known_failure.error->retryable);

    const auto unknown_failure = prediction.score_result({}, "observed", "now");
    assert(unknown_failure.valid());
    assert(!unknown_failure.success);
    assert(unknown_failure.error);
    assert(unknown_failure.error->operation == "prediction.score");
    assert(unknown_failure.error->code == "unknown_adapter_delegation_error");
}
