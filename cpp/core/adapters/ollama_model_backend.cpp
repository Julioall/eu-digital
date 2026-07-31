#include "core/adapters/ollama_model_backend.hpp"
#include <sstream>
#include <iostream>
#include <vector>

#pragma comment(lib, "winhttp.lib")

namespace eu_digital {

OllamaModelBackend::OllamaModelBackend() {
#ifdef _WIN32
    hSession_ = WinHttpOpen(L"EU-Digital Ollama Backend/1.0", 
                            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, 
                            WINHTTP_NO_PROXY_NAME, 
                            WINHTTP_NO_PROXY_BYPASS, 0);
    if (hSession_) {
        hConnect_ = WinHttpConnect(hSession_, L"127.0.0.1", 11434, 0);
    }
#endif
}

OllamaModelBackend::~OllamaModelBackend() {
#ifdef _WIN32
    if (hConnect_) WinHttpCloseHandle(hConnect_);
    if (hSession_) WinHttpCloseHandle(hSession_);
#endif
}

const std::string& OllamaModelBackend::backend_id() const {
    return backend_id_;
}

void OllamaModelBackend::load(const LocalModelArtifact& artifact) {
    std::lock_guard lock(mutex_);
    current_model_ = artifact.model_id;
    // For Ollama, the model is already downloaded locally or pulled on demand.
    // If we want to be strict, we could call `/api/show` to verify it exists.
}

void OllamaModelBackend::unload(const std::string& model_id) {
    std::lock_guard lock(mutex_);
    if (current_model_ == model_id) {
        current_model_.clear();
    }
}

void OllamaModelBackend::cancel(const std::string& request_id) {
    std::lock_guard lock(mutex_);
    auto it = active_requests_.find(request_id);
    if (it != active_requests_.end()) {
        it->second.cancelled = true;
#ifdef _WIN32
        if (it->second.hRequest) {
            WinHttpCloseHandle(it->second.hRequest);
            it->second.hRequest = nullptr;
        }
#endif
    }
}

LocalModelRawOutput OllamaModelBackend::invoke(const LocalModelRequest& request) {
    // Construct the Ollama JSON payload
    std::ostringstream payload;
    payload << "{\"model\":\"" << request.model_id << "\",";
    payload << "\"prompt\":\"" << local_model_json_escape(request.rendered_prompt) << "\",";
    payload << "\"stream\":false}";

    std::string response = post_json("/api/generate", payload.str(), request.timeout_seconds, request.request_id);
    
    // Parse the response manually for 'response'
    // A real parser would be better, but we do simple string matching for now to avoid json deps
    std::string output_text;
    auto res_pos = response.find("\"response\":\"");
    if (res_pos != std::string::npos) {
        auto start = res_pos + 12;
        auto end = response.find("\"", start);
        // Extremely naive unescape for typical responses, a robust parser should be used.
        while (end != std::string::npos && response[end-1] == '\\') {
            end = response.find("\"", end + 1);
        }
        if (end != std::string::npos) {
            output_text = response.substr(start, end - start);
        }
    }

    LocalModelRawOutput output;
    output.kind = "generation";
    output.fields["text"] = output_text;
    return output;
}

std::string OllamaModelBackend::post_json(const std::string& endpoint, const std::string& payload, double timeout_seconds, const std::string& request_id) {
#ifndef _WIN32
    throw LocalModelGatewayError("Ollama backend is currently only implemented for Windows (WinHTTP).");
#else
    if (!hConnect_) throw LocalModelGatewayError("WinHTTP not connected.");

    std::wstring wEndpoint(endpoint.begin(), endpoint.end());
    
    HINTERNET hRequest = WinHttpOpenRequest(hConnect_, L"POST", wEndpoint.c_str(), 
                                            nullptr, WINHTTP_NO_REFERER, 
                                            WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
    if (!hRequest) {
        throw LocalModelGatewayError("Failed to open WinHTTP request.");
    }

    {
        std::lock_guard lock(mutex_);
        active_requests_[request_id] = {hRequest, false};
    }

    int timeout_ms = static_cast<int>(timeout_seconds * 1000.0);
    WinHttpSetTimeouts(hRequest, timeout_ms, timeout_ms, timeout_ms, timeout_ms);

    LPCWSTR headers = L"Content-Type: application/json\r\n";
    BOOL sent = WinHttpSendRequest(hRequest, headers, -1L, 
                                   (LPVOID)payload.c_str(), 
                                   (DWORD)payload.length(), 
                                   (DWORD)payload.length(), 0);
    
    if (!sent) {
        std::lock_guard lock(mutex_);
        if (active_requests_[request_id].cancelled) {
            active_requests_.erase(request_id);
            throw LocalModelCancelledError("Request cancelled.");
        }
        WinHttpCloseHandle(hRequest);
        active_requests_.erase(request_id);
        throw LocalModelGatewayError("Failed to send WinHTTP request.");
    }

    BOOL received = WinHttpReceiveResponse(hRequest, nullptr);
    if (!received) {
        std::lock_guard lock(mutex_);
        if (active_requests_[request_id].cancelled) {
            active_requests_.erase(request_id);
            throw LocalModelCancelledError("Request cancelled.");
        }
        WinHttpCloseHandle(hRequest);
        active_requests_.erase(request_id);
        throw LocalModelTimeoutError("WinHTTP receive timeout.");
    }

    std::string response_data;
    DWORD size = 0;
    do {
        if (!WinHttpQueryDataAvailable(hRequest, &size)) break;
        if (size == 0) break;
        
        std::vector<char> buffer(size);
        DWORD downloaded = 0;
        if (WinHttpReadData(hRequest, buffer.data(), size, &downloaded)) {
            response_data.append(buffer.data(), downloaded);
        }
    } while (size > 0);

    {
        std::lock_guard lock(mutex_);
        if (active_requests_[request_id].cancelled) {
            active_requests_.erase(request_id);
            throw LocalModelCancelledError("Request cancelled.");
        }
        WinHttpCloseHandle(hRequest);
        active_requests_.erase(request_id);
    }

    return response_data;
#endif
}

} // namespace eu_digital
