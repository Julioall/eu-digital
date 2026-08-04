#include "core/adapters/local_language_renderer.hpp"

#include <atomic>
#include <cassert>
#include <chrono>
#include <stop_token>
#include <string>
#include <thread>

using namespace eu_digital;

namespace {

contracts::CognitiveOutputRequestV1 request(
    contracts::CognitiveOutputIntentV1 intent =
        contracts::CognitiveOutputIntentV1::requested_response) {
    contracts::CognitiveOutputRequestV1 value;
    value.request_id = "request-1";
    value.correlation_id = "correlation-1";
    value.input_event_id = "event-1";
    value.intent = intent;
    value.occurred_at = "2026-08-04T12:00:00Z";
    value.critical = intent == contracts::CognitiveOutputIntentV1::question ||
                     intent ==
                         contracts::CognitiveOutputIntentV1::requested_response;
    value.reason = "explicit_user_request";
    value.input_content = {{"text", "Olá"}};
    value.self_model_id = "self-1";
    value.self_constraints = {"local_only"};
    value.evidence_refs = {"event-1", "memory-1"};
    assert(value.valid());
    return value;
}

std::string candidate(std::string extra = {},
                      std::string evidence = "event-1") {
    return "{\"schema_version\":\"1.0\",\"request_id\":\"request-1\","
           "\"intent\":\"requested_response\",\"rendered_text\":\"Olá\","
           "\"evidence_refs\":[\"" + evidence + "\"]" + extra + "}";
}

}  // namespace

int main() {
    std::string captured_prompt;
    LocalLanguageRenderer renderer(
        [&](const std::string& prompt) {
            captured_prompt = prompt;
            return candidate();
        });
    const auto rendered = renderer.render(request());
    assert(rendered.valid());
    assert(rendered.status == contracts::DialogueOutputStatusV1::rendered);
    assert(rendered.rendered_text == "Olá");
    assert(captured_prompt.find("local_only") != std::string::npos);
    assert(captured_prompt.find("COGNITIVE_OUTPUT_REQUEST_1_0") !=
           std::string::npos);

    LocalLanguageRenderer unknown_field(
        [](const std::string&) { return candidate(",\"extra\":true"); });
    const auto strict = unknown_field.render(request());
    assert(strict.valid());
    assert(strict.status ==
           contracts::DialogueOutputStatusV1::fallback_used);
    assert(strict.reason_code == "candidate_malformed");

    LocalLanguageRenderer foreign_evidence(
        [](const std::string&) { return candidate({}, "outside-request"); });
    const auto rejected = foreign_evidence.render(request());
    assert(rejected.status ==
           contracts::DialogueOutputStatusV1::fallback_used);
    assert(rejected.reason_code == "candidate_contract_violation");

    auto proactive = request(
        contracts::CognitiveOutputIntentV1::proactive_suggestion);
    proactive.request_id = "proactive-1";
    LocalLanguageRenderer malformed([](const std::string&) {
        return std::string("not-json");
    });
    const auto quiet = malformed.render(proactive);
    assert(quiet.valid());
    assert(quiet.status == contracts::DialogueOutputStatusV1::silence);
    assert(quiet.rendered_text.empty());

    std::atomic<bool> cancellation_seen{false};
    const auto timeout_started = std::chrono::steady_clock::now();
    {
        LocalLanguageRenderer timeout(
            [&](const std::string&, std::stop_token token) {
                while (!token.stop_requested()) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
                cancellation_seen = true;
                return candidate();
            },
            std::chrono::milliseconds(10));
        const auto fallback = timeout.render(request());
        assert(fallback.status ==
               contracts::DialogueOutputStatusV1::fallback_used);
        assert(fallback.reason_code == "renderer_timeout");
    }
    const auto timeout_elapsed = std::chrono::steady_clock::now() - timeout_started;
    assert(timeout_elapsed < std::chrono::milliseconds(60));
    for (int attempt = 0; attempt < 50 && !cancellation_seen; ++attempt) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    assert(cancellation_seen);

    const auto uncooperative_started = std::chrono::steady_clock::now();
    {
        LocalLanguageRenderer uncooperative(
            [](const std::string&, std::stop_token) {
                std::this_thread::sleep_for(std::chrono::milliseconds(150));
                return candidate();
            },
            std::chrono::milliseconds(10));
        const auto first = uncooperative.render(request());
        assert(first.reason_code == "renderer_timeout");
        const auto second = uncooperative.render(request());
        assert(second.reason_code == "renderer_busy");
    }
    assert(std::chrono::steady_clock::now() - uncooperative_started <
           std::chrono::milliseconds(70));

    LocalLanguageRenderer unavailable(
        LocalLanguageRenderer::GenerationFunction{},
        std::chrono::milliseconds(10));
    assert(unavailable.render(request()).reason_code == "renderer_unavailable");

    auto silence_request = request(contracts::CognitiveOutputIntentV1::silence);
    silence_request.request_id = "silence-1";
    silence_request.critical = false;
    const auto silence = renderer.render(silence_request);
    assert(silence.valid());
    assert(silence.status == contracts::DialogueOutputStatusV1::silence);
}
