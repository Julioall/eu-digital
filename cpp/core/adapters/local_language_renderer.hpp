#pragma once

#include "core/ports/ilanguage_renderer.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <stop_token>
#include <string>
#include <thread>

namespace eu_digital {

class LocalLanguageRenderer final : public ILanguageRenderer {
public:
    using GenerationFunction = std::function<std::string(
        const std::string&, std::stop_token)>;
    using LegacyGenerationFunction =
        std::function<std::string(const std::string&)>;

    explicit LocalLanguageRenderer(
        GenerationFunction generation,
        std::chrono::milliseconds timeout = std::chrono::milliseconds(3000),
        std::string renderer_id = "local-language-renderer-v1")
        : generation_(std::move(generation)),
          timeout_(timeout),
          renderer_id_(std::move(renderer_id)),
          state_(std::make_shared<SharedRendererState>()) {
        if (timeout_ <= std::chrono::milliseconds::zero()) {
            throw std::invalid_argument("renderer timeout must be positive");
        }
        if (renderer_id_.empty()) {
            throw std::invalid_argument("renderer_id cannot be empty");
        }
    }

    explicit LocalLanguageRenderer(
        LegacyGenerationFunction generation,
        int timeout_ms = 3000,
        std::string renderer_id = "local-language-renderer-v1")
        : LocalLanguageRenderer(
              [generation = std::move(generation)](
                  const std::string& prompt, std::stop_token) {
                  return generation ? generation(prompt) : std::string{};
              },
              std::chrono::milliseconds(timeout_ms),
              std::move(renderer_id)) {}

    contracts::ValidatedDialogueOutputV1 render(
        const contracts::CognitiveOutputRequestV1& request) override {
        if (!request.valid()) {
            throw std::invalid_argument("invalid cognitive output request");
        }
        if (request.intent == contracts::CognitiveOutputIntentV1::silence) {
            return contracts::ValidatedDialogueOutputV1::silence(
                request, renderer_id_, "silence_intent");
        }
        if (!generation_) return failure(request, "renderer_unavailable");

        bool expected = false;
        if (!state_->in_flight.compare_exchange_strong(expected, true)) {
            return failure(request, "renderer_busy");
        }

        const auto prompt = build_prompt(request);
        const auto generation = generation_;
        const auto state = state_;
        const auto result = std::make_shared<CallResult>();
        std::stop_source stop_source;
        const auto token = stop_source.get_token();

        std::thread worker([generation, prompt, state, result, token] {
            try {
                result->raw = generation(prompt, token);
            } catch (...) {
                result->error = std::current_exception();
            }
            {
                std::lock_guard lock(result->mutex);
                result->done = true;
            }
            state->in_flight.store(false);
            result->ready.notify_all();
        });

        {
            std::unique_lock lock(result->mutex);
            if (!result->ready.wait_for(lock, timeout_,
                                        [&] { return result->done; })) {
                stop_source.request_stop();
                worker.detach();
                return failure(request, "renderer_timeout");
            }
        }
        worker.join();
        if (result->error) return failure(request, "renderer_exception");

        try {
            const auto candidate =
                contracts::LanguageRenderingCandidateV1::parse_strict(result->raw);
            if (!candidate.valid_for(request)) {
                return failure(request, "candidate_contract_violation");
            }
            return contracts::ValidatedDialogueOutputV1::from_candidate(
                request, candidate, renderer_id_);
        } catch (const std::exception&) {
            return failure(request, "candidate_malformed");
        }
    }

    static std::string build_prompt(
        const contracts::CognitiveOutputRequestV1& request) {
        if (!request.valid()) {
            throw std::invalid_argument("invalid cognitive output request");
        }
        return
            "You are a local language renderer, not a decision maker. "
            "Use only input_content, self_constraints and evidence_refs from "
            "the request. Do not invent factual claims or evidence. Return "
            "exactly one JSON object with only schema_version, request_id, "
            "intent, rendered_text and evidence_refs. Copy request_id and "
            "intent exactly. evidence_refs must be a subset of the request.\n"
            "COGNITIVE_OUTPUT_REQUEST_1_0=" + request.to_json();
    }

private:
    struct SharedRendererState {
        std::atomic<bool> in_flight{false};
    };

    struct CallResult {
        std::mutex mutex;
        std::condition_variable ready;
        bool done{false};
        std::string raw;
        std::exception_ptr error;
    };

    contracts::ValidatedDialogueOutputV1 failure(
        const contracts::CognitiveOutputRequestV1& request,
        const std::string& reason_code) const {
        if (request.critical) {
            return contracts::ValidatedDialogueOutputV1::fallback(
                request, renderer_id_, reason_code);
        }
        return contracts::ValidatedDialogueOutputV1::silence(
            request, renderer_id_, reason_code);
    }

    GenerationFunction generation_;
    std::chrono::milliseconds timeout_;
    std::string renderer_id_;
    std::shared_ptr<SharedRendererState> state_;
};

}  // namespace eu_digital
