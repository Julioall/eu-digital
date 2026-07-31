#pragma once

#include "core/local_model_gateway.hpp"

#include <string>
#include <mutex>
#include <atomic>

#ifdef _WIN32
#include <windows.h>
#include <winhttp.h>
#endif

namespace eu_digital {

class OllamaModelBackend : public LocalModelBackend {
public:
    OllamaModelBackend();
    ~OllamaModelBackend() override;

    const std::string& backend_id() const override;
    void load(const LocalModelArtifact& artifact) override;
    LocalModelRawOutput invoke(const LocalModelRequest& request) override;
    void cancel(const std::string& request_id) override;
    void unload(const std::string& model_id) override;

private:
    std::string post_json(const std::string& endpoint, const std::string& payload, double timeout_seconds, const std::string& request_id);
    
    std::string backend_id_{"ollama"};
    
#ifdef _WIN32
    HINTERNET hSession_{nullptr};
    HINTERNET hConnect_{nullptr};
#endif

    std::mutex mutex_;
    std::string current_model_;
    
    // To support cancellation
    struct ActiveRequest {
#ifdef _WIN32
        HINTERNET hRequest{nullptr};
#endif
        bool cancelled{false};
    };
    std::map<std::string, ActiveRequest> active_requests_;
};

} // namespace eu_digital
