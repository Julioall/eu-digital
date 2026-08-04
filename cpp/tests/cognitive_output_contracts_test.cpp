#include "core/contracts/cognitive_output.hpp"

#include <cassert>
#include <stdexcept>
#include <string>

using namespace eu_digital::contracts;

namespace {

CognitiveOutputRequestV1 request() {
    CognitiveOutputRequestV1 value;
    value.request_id = "request-1";
    value.correlation_id = "correlation-1";
    value.input_event_id = "event-1";
    value.intent = CognitiveOutputIntentV1::requested_response;
    value.occurred_at = "2026-08-04T12:00:00Z";
    value.critical = true;
    value.reason = "explicit_user_request";
    value.input_content = {{"text", "Olá"}};
    value.self_model_id = "self-1";
    value.self_constraints = {"local_only"};
    value.evidence_refs = {"event-1", "memory-1"};
    return value;
}

bool rejects(const std::string& value) {
    try {
        (void)LanguageRenderingCandidateV1::parse_strict(value);
        return false;
    } catch (const std::invalid_argument&) {
        return true;
    }
}

}  // namespace

int main() {
    auto value = request();
    assert(value.valid());
    const auto json = value.to_json();
    assert(json ==
        "{\"correlation_id\":\"correlation-1\",\"critical\":true,"
        "\"evidence_refs\":[\"event-1\",\"memory-1\"],"
        "\"input_content\":{\"text\":\"Olá\"},"
        "\"input_event_id\":\"event-1\","
        "\"intent\":\"requested_response\","
        "\"occurred_at\":\"2026-08-04T12:00:00Z\","
        "\"reason\":\"explicit_user_request\","
        "\"request_id\":\"request-1\",\"schema_version\":\"1.0\","
        "\"self_constraints\":[\"local_only\"],"
        "\"self_model_id\":\"self-1\"}");

    const std::string candidate_json =
        "{\"schema_version\":\"1.0\",\"request_id\":\"request-1\","
        "\"intent\":\"requested_response\",\"rendered_text\":\"Olá\","
        "\"evidence_refs\":[\"event-1\"]}";
    const auto candidate =
        LanguageRenderingCandidateV1::parse_strict(candidate_json);
    assert(candidate.valid_for(value));
    const auto output = ValidatedDialogueOutputV1::from_candidate(
        value, candidate, "renderer-1");
    assert(output.valid());
    assert(output.presentable());

    assert(rejects(candidate_json.substr(0, candidate_json.size() - 1) +
                   ",\"extra\":true}"));
    assert(rejects(candidate_json.substr(0, candidate_json.size() - 1)));
    assert(rejects(
        "{\"schema_version\":\"1.0\",\"schema_version\":\"1.0\","
        "\"request_id\":\"request-1\",\"intent\":\"requested_response\","
        "\"rendered_text\":\"Olá\",\"evidence_refs\":[]}"));
    assert(rejects("not-json"));

    auto foreign = candidate;
    foreign.evidence_refs = {"outside-request"};
    assert(!foreign.valid_for(value));
    auto wrong_intent = candidate;
    wrong_intent.intent = CognitiveOutputIntentV1::question;
    assert(!wrong_intent.valid_for(value));

    value.critical = false;
    assert(!value.valid());
    value.critical = true;
    value.evidence_refs.push_back("event-1");
    assert(!value.valid());
}
