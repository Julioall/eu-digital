#include "core/functional_self_model.hpp"

#include <cassert>
#include <stdexcept>

namespace {

eu_digital::FunctionalSelfModelEvent capability_event(
    const std::string& event_id, const std::string& capability_id,
    const std::string& status, const std::string& occurred_at) {
    eu_digital::FunctionalSelfModelCapability capability{
        capability_id, status, "Capability " + capability_id + " is declared " + status + ".",
        {"source-" + event_id}};
    return {
        event_id, "1.0", occurred_at, "capability_changed",
        capability_id + " is " + status, {"source-" + event_id},
        capability, std::nullopt};
}

eu_digital::FunctionalSelfModelEvent assertion_event(
    const std::string& event_id, const std::string& classification,
    const std::string& occurred_at) {
    eu_digital::FunctionalSelfModelAssertion assertion{
        "assertion-" + event_id, "local-agent", "mode", "value-" + classification,
        classification, "Recorded " + classification + " for test.",
        {"source-" + event_id}};
    return {
        event_id, "1.0", occurred_at, "assertion_recorded",
        "record " + classification, {"source-" + event_id},
        std::nullopt, assertion};
}

void versioned_updates_preserve_history() {
    eu_digital::VersionedFunctionalSelfModel model("local-agent", "2026-07-28T12:00:00+00:00");
    const auto initial = model.current();
    const auto available = model.apply(capability_event(
        "available", "screen.capture", "available", "2026-07-28T12:00:00+00:00"));
    const auto unavailable = model.apply(capability_event(
        "unavailable", "screen.capture", "unavailable", "2026-07-28T12:00:01+00:00"));
    assert(initial.version == 0);
    assert(available.version == 1);
    assert(unavailable.version == 2);
    assert(model.version(1).capabilities.front().status == "available");
    assert(model.version(2).capabilities.front().status == "unavailable");
    assert(model.version(0).capabilities.empty());
}

void decisions_gate_capabilities_and_baseline_is_explicit() {
    eu_digital::VersionedFunctionalSelfModel treatment("treatment", "2026-07-28T12:00:00+00:00");
    assert(!treatment.decide("screen.capture").allowed);
    assert(treatment.decide("screen.capture").reason_code == "capability_unverified");
    treatment.apply(capability_event(
        "available", "screen.capture", "available", "2026-07-28T12:00:00+00:00"));
    assert(treatment.decide("screen.capture").allowed);
    treatment.apply(capability_event(
        "removed", "screen.capture", "removed", "2026-07-28T12:00:01+00:00"));
    assert(!treatment.decide("screen.capture").allowed);
    assert(treatment.decide("screen.capture").reason_code == "capability_removed");

    eu_digital::VersionedFunctionalSelfModel baseline(
        "baseline", "2026-07-28T12:00:00+00:00",
        eu_digital::FUNCTIONAL_SELF_MODEL_BASELINE_ID);
    baseline.apply(capability_event(
        "removed", "screen.capture", "removed", "2026-07-28T12:00:00+00:00"));
    const auto decision = baseline.decide("screen.capture");
    assert(decision.allowed);
    assert(decision.policy_id == eu_digital::FUNCTIONAL_SELF_MODEL_BASELINE_ID);
}

void assertions_and_lifecycle_are_typed() {
    eu_digital::VersionedFunctionalSelfModel model("assertions", "2026-07-28T12:00:00+00:00");
    model.apply(assertion_event("fact", "fact", "2026-07-28T12:00:00+00:00"));
    model.apply(assertion_event("hypothesis", "hypothesis", "2026-07-28T12:00:01+00:00"));
    const auto current = model.apply(assertion_event(
        "configuration", "configuration", "2026-07-28T12:00:02+00:00"));
    assert(current.facts.size() == 1);
    assert(current.hypotheses.size() == 1);
    assert(current.configuration.size() == 1);

    bool duplicate_rejected = false;
    try {
        model.apply(assertion_event("configuration", "configuration", "2026-07-28T00:00:03+00:00"));
    } catch (const std::invalid_argument&) {
        duplicate_rejected = true;
    }
    assert(duplicate_rejected);

    eu_digital::FunctionalSelfModelPlugin plugin;
    assert(plugin.descriptor().valid());
    assert(plugin.descriptor().supports_hot_plug);
    assert(plugin.descriptor().provides_operation("decide.self_model"));
}

}  // namespace

int main() {
    versioned_updates_preserve_history();
    decisions_gate_capabilities_and_baseline_is_explicit();
    assertions_and_lifecycle_are_typed();
    return 0;
}
