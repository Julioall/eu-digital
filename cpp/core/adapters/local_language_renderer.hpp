#pragma once

#include "core/ports/ilanguage_renderer.hpp"
#include <future>
#include <chrono>
#include <string>
#include <functional>

namespace eu_digital {

// In a real implementation, this would be injected via a port to access the local LLM.
// For this adapter, we assume an internal function or a mockable dependency.
class LocalLanguageRenderer final : public ILanguageRenderer {
public:
    using LlmFunction = std::function<std::string(const std::string&)>;

    explicit LocalLanguageRenderer(LlmFunction llm_func, int timeout_ms = 3000)
        : llm_func_(std::move(llm_func)), timeout_ms_(timeout_ms) {}

    ValidatedDialogueOutput render(const CognitiveOutputRequest& request) override {
        if (request.intent == "silence") {
            return ValidatedDialogueOutput::silence();
        }

        // Construct a prompt based on the request constraints
        std::string prompt = "Constraint: " + request.self_constraint_snapshot + "\n" +
                             "Intent: " + request.intent + "\n" +
                             "Params: " + request.prompt_parameters;

        // Execute LLM call asynchronously to allow for timeout
        auto future = std::async(std::launch::async, [this, prompt]() {
            if (llm_func_) {
                return llm_func_(prompt);
            }
            return std::string("");
        });

        // Wait for result with timeout
        auto status = future.wait_for(std::chrono::milliseconds(timeout_ms_));

        if (status == std::future_status::timeout) {
            // Detach or let the future finish in background, we return fallback
            return ValidatedDialogueOutput::fallback("Desculpe, estou demorando para processar a informação.");
        }

        std::string llm_output = future.get();

        // Simulate parsing JSON. In a real system we would use a JSON library and 
        // validate against cognitive_output.schema.json
        if (llm_output.find("malformed") != std::string::npos || llm_output.empty()) {
            return ValidatedDialogueOutput::fallback("Não consegui formular uma resposta clara.");
        }

        return ValidatedDialogueOutput::success(llm_output);
    }

private:
    LlmFunction llm_func_;
    int timeout_ms_;
};

} // namespace eu_digital
