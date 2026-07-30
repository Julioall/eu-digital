#pragma once

#include "core/capability_runtime.hpp"
#include "core/digest.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <charconv>
#include <chrono>
#include <condition_variable>
#include <cmath>
#include <cstdint>
#include <exception>
#include <future>
#include <iomanip>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace eu_digital {

inline constexpr const char* LOCAL_MODEL_SCHEMA_VERSION = "1.0";
inline constexpr const char* LOCAL_MODEL_PRIORITY_SCHEDULER_ID = "priority_single_worker_v1";
inline constexpr const char* LOCAL_MODEL_BASELINE_SCHEDULER_ID = "fifo_single_worker_v0";
inline constexpr const char* LOCAL_MODEL_HYPOTHESIS =
    "a single-worker local gateway with stable priority ordering preserves the one-heavy-model resource bound while reducing priority wait versus FIFO";
inline constexpr const char* LOCAL_MODEL_ABLATION =
    "select fifo_single_worker_v0 through the same gateway interface while retaining the single-worker resource bound";
inline constexpr const char* LOCAL_MODEL_FALSIFICATION =
    "two heavy inferences or loaded models coexist, a timeout retains a resource, or invalid structured output is returned to a caller";
inline constexpr const char* LOCAL_MODEL_NAMESPACE = "8272847f-ce45-4630-8ec5-5ca8b5a83c49";
inline constexpr std::uint64_t LOCAL_MODEL_MAX_BYTES = 4ULL * 1024ULL * 1024ULL * 1024ULL;

class LocalModelGatewayError : public std::runtime_error {
public:
    explicit LocalModelGatewayError(const std::string& message) : std::runtime_error(message) {}
};

class LocalModelTimeoutError : public LocalModelGatewayError {
public:
    explicit LocalModelTimeoutError(const std::string& message) : LocalModelGatewayError(message) {}
};

class LocalModelCancelledError : public LocalModelGatewayError {
public:
    explicit LocalModelCancelledError(const std::string& message) : LocalModelGatewayError(message) {}
};

class InvalidLocalModelResponseError : public LocalModelGatewayError {
public:
    explicit InvalidLocalModelResponseError(const std::string& message) : LocalModelGatewayError(message) {}
};

class LocalModelArtifactError : public LocalModelGatewayError {
public:
    explicit LocalModelArtifactError(const std::string& message) : LocalModelGatewayError(message) {}
};

enum class LocalModelSchedulingPolicy { priority_single_worker_v1, fifo_single_worker_v0 };

inline std::string local_model_policy_string(LocalModelSchedulingPolicy policy) {
    return policy == LocalModelSchedulingPolicy::priority_single_worker_v1
        ? LOCAL_MODEL_PRIORITY_SCHEDULER_ID : LOCAL_MODEL_BASELINE_SCHEDULER_ID;
}

inline std::string local_model_json_escape(const std::string& value) {
    std::ostringstream output;
    for (const auto character : value) {
        switch (character) {
        case '"': output << "\\\""; break;
        case '\\': output << "\\\\"; break;
        case '\b': output << "\\b"; break;
        case '\f': output << "\\f"; break;
        case '\n': output << "\\n"; break;
        case '\r': output << "\\r"; break;
        case '\t': output << "\\t"; break;
        default: output << character; break;
        }
    }
    return output.str();
}

inline std::string local_model_json_string(const std::string& value) {
    return "\"" + local_model_json_escape(value) + "\"";
}

inline std::string local_model_json_number(double value) {
    if (!std::isfinite(value)) throw LocalModelGatewayError("non-finite number cannot be serialized");
    std::array<char, 64> buffer{};
    const auto conversion = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
    if (conversion.ec != std::errc{}) throw LocalModelGatewayError("number cannot be formatted");
    auto result = std::string(buffer.data(), conversion.ptr);
    if (result.find_first_of(".eE") == std::string::npos) result += ".0";
    return result;
}

inline std::string local_model_json_bool(bool value) { return value ? "true" : "false"; }

inline void local_model_required(const std::string& value, const char* name) {
    if (value.empty()) throw LocalModelGatewayError(std::string(name) + " must be a non-empty string");
}

inline std::string local_model_join(const std::vector<std::string>& values) {
    std::ostringstream output;
    for (std::size_t index = 0; index < values.size(); ++index) {
        if (index) output << ',';
        output << values[index];
    }
    return output.str();
}

inline bool local_model_hex_digest(const std::string& value) {
    if (value.size() != 64) return false;
    return std::all_of(value.begin(), value.end(), [](char character) {
        return std::isdigit(static_cast<unsigned char>(character)) ||
            (std::tolower(static_cast<unsigned char>(character)) >= 'a' &&
             std::tolower(static_cast<unsigned char>(character)) <= 'f');
    });
}

inline std::string local_model_string_array(const std::vector<std::string>& values) {
    std::ostringstream output;
    output << '[';
    for (std::size_t index = 0; index < values.size(); ++index) {
        if (index) output << ',';
        output << local_model_json_string(values[index]);
    }
    output << ']';
    return output.str();
}

struct ModelPromptTemplate {
    std::string template_id;
    std::string version;
    std::string body;
    std::vector<std::string> variables;

    void validate() const {
        local_model_required(template_id, "template_id");
        local_model_required(version, "template_version");
        local_model_required(body, "template_body");
        std::set<std::string> declared;
        for (const auto& variable : variables) {
            local_model_required(variable, "template_variable");
            if (!declared.insert(variable).second) throw LocalModelGatewayError("template variables must be unique");
        }
        std::set<std::string> referenced;
        for (std::size_t index = 0; index < body.size();) {
            if (body[index] != '{') {
                ++index;
                continue;
            }
            if (index + 1 < body.size() && body[index + 1] == '{') {
                index += 2;
                continue;
            }
            const auto end = body.find('}', index + 1);
            if (end == std::string::npos || end == index + 1) throw LocalModelGatewayError("template placeholder is invalid");
            const auto name = body.substr(index + 1, end - index - 1);
            if (name.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_") != std::string::npos ||
                std::isdigit(static_cast<unsigned char>(name.front()))) {
                throw LocalModelGatewayError("template placeholders must be simple identifiers");
            }
            referenced.insert(name);
            index = end + 1;
        }
        if (referenced != declared) throw LocalModelGatewayError("template variables must match template placeholders");
    }

    std::string render(const std::map<std::string, std::string>& values) const {
        validate();
        std::set<std::string> declared(variables.begin(), variables.end());
        std::set<std::string> provided;
        for (const auto& [key, unused] : values) provided.insert(key);
        if (declared != provided) throw LocalModelGatewayError("template values must exactly match declared variables");
        std::ostringstream output;
        for (std::size_t index = 0; index < body.size();) {
            if (body[index] == '{' && index + 1 < body.size() && body[index + 1] == '{') {
                output << '{';
                index += 2;
                continue;
            }
            if (body[index] == '}' && index + 1 < body.size() && body[index + 1] == '}') {
                output << '}';
                index += 2;
                continue;
            }
            if (body[index] != '{') {
                output << body[index++];
                continue;
            }
            const auto end = body.find('}', index + 1);
            if (end == std::string::npos) throw LocalModelGatewayError("template rendering failed");
            output << values.at(body.substr(index + 1, end - index - 1));
            index = end + 1;
        }
        return output.str();
    }

    std::string to_json() const {
        validate();
        std::ostringstream output;
        output << "{\"body\":" << local_model_json_string(body)
               << ",\"template_id\":" << local_model_json_string(template_id)
               << ",\"variables\":" << local_model_string_array(variables)
               << ",\"version\":" << local_model_json_string(version) << '}';
        return output.str();
    }
};

struct LocalModelRequest {
    std::string request_id;
    std::string backend_id;
    std::string model_id;
    int priority{0};
    double timeout_seconds{1.0};
    ModelPromptTemplate template_value;
    std::string rendered_prompt;
    std::string schema_version{LOCAL_MODEL_SCHEMA_VERSION};

    void validate() const {
        local_model_required(request_id, "request_id");
        local_model_required(backend_id, "backend_id");
        local_model_required(model_id, "model_id");
        if (priority < 0) throw LocalModelGatewayError("priority must be non-negative");
        if (!std::isfinite(timeout_seconds) || timeout_seconds <= 0.0) throw LocalModelGatewayError("timeout_seconds must be positive");
        local_model_required(rendered_prompt, "rendered_prompt");
        if (schema_version != LOCAL_MODEL_SCHEMA_VERSION) throw LocalModelGatewayError("unsupported model request schema version");
        template_value.validate();
    }

    std::string to_json() const {
        validate();
        std::ostringstream output;
        output << "{\"backend_id\":" << local_model_json_string(backend_id)
               << ",\"model_id\":" << local_model_json_string(model_id)
               << ",\"priority\":" << priority
               << ",\"rendered_prompt\":" << local_model_json_string(rendered_prompt)
               << ",\"request_id\":" << local_model_json_string(request_id)
               << ",\"schema_version\":" << local_model_json_string(schema_version)
               << ",\"template\":{\"template_id\":" << local_model_json_string(template_value.template_id)
               << ",\"version\":" << local_model_json_string(template_value.version) << "}"
               << ",\"timeout_seconds\":" << local_model_json_number(timeout_seconds) << '}';
        return output.str();
    }
};

struct LocalModelRawOutput {
    std::string kind;
    std::map<std::string, std::string> fields;
};

inline std::string local_model_fields_json(const std::map<std::string, std::string>& fields) {
    std::ostringstream output;
    output << '{';
    bool first = true;
    for (const auto& [key, value] : fields) {
        if (!first) output << ',';
        first = false;
        output << local_model_json_string(key) << ':' << local_model_json_string(value);
    }
    output << '}';
    return output.str();
}

struct LocalModelResponse {
    std::string response_id;
    std::string request_id;
    std::string backend_id;
    std::string model_id;
    std::string output_kind;
    std::map<std::string, std::string> output_fields;
    double latency_ms{0.0};
    std::string schema_version{LOCAL_MODEL_SCHEMA_VERSION};

    void validate() const {
        local_model_required(response_id, "response_id");
        local_model_required(request_id, "request_id");
        local_model_required(backend_id, "backend_id");
        local_model_required(model_id, "model_id");
        local_model_required(output_kind, "output_kind");
        if (!std::isfinite(latency_ms) || latency_ms < 0.0) throw LocalModelGatewayError("latency_ms must be non-negative");
        if (schema_version != LOCAL_MODEL_SCHEMA_VERSION) throw LocalModelGatewayError("unsupported model response schema version");
    }

    std::string to_json() const {
        validate();
        std::ostringstream output;
        output << "{\"backend_id\":" << local_model_json_string(backend_id)
               << ",\"latency_ms\":" << local_model_json_number(latency_ms)
               << ",\"model_id\":" << local_model_json_string(model_id)
               << ",\"output\":{\"fields\":" << local_model_fields_json(output_fields)
               << ",\"kind\":" << local_model_json_string(output_kind) << "}"
               << ",\"request_id\":" << local_model_json_string(request_id)
               << ",\"response_id\":" << local_model_json_string(response_id)
               << ",\"schema_version\":" << local_model_json_string(schema_version)
               << ",\"status\":\"completed\"}";
        return output.str();
    }
};

struct LocalModelArtifact {
    std::string model_id;
    std::string format{"GGUF"};
    std::string quantization;
    std::uint64_t size_bytes{0};
    std::string sha256;
    std::string language;
    std::string license_id;
    bool license_compatible{false};
    std::string backend_compatibility;
    std::string signature_algorithm{"detached_manifest_digest_v1"};
    std::string signature;
    std::string signing_key_id;
    std::string runtime_artifact_id;
    std::string payload_artifact_id;
    bool payload_separate{true};

    std::string expected_signature() const {
        const auto input = std::string("eu-digital-model-signature-v1:") + signing_key_id + ":" +
            model_id + ":" + sha256 + ":" + runtime_artifact_id + ":" + payload_artifact_id;
        return digest::hex(digest::sha256(input));
    }

    void validate() const {
        local_model_required(model_id, "model_id");
        if (format != "GGUF") throw LocalModelArtifactError("model format must be GGUF");
        local_model_required(quantization, "quantization");
        if (size_bytes == 0 || size_bytes > LOCAL_MODEL_MAX_BYTES) throw LocalModelArtifactError("model size exceeds the 4 GiB policy");
        if (!local_model_hex_digest(sha256)) throw LocalModelArtifactError("model sha256 is invalid");
        if (language.rfind("pt", 0) != 0) throw LocalModelArtifactError("model language is not Portuguese-compatible");
        local_model_required(license_id, "license_id");
        if (!license_compatible) throw LocalModelArtifactError("model license is not compatible");
        local_model_required(backend_compatibility, "backend_compatibility");
        if (signature_algorithm != "detached_manifest_digest_v1" || !local_model_hex_digest(signature)) {
            throw LocalModelArtifactError("model detached signature is invalid");
        }
        local_model_required(signing_key_id, "signing_key_id");
        local_model_required(runtime_artifact_id, "runtime_artifact_id");
        local_model_required(payload_artifact_id, "payload_artifact_id");
        if (runtime_artifact_id == payload_artifact_id || !payload_separate) {
            throw LocalModelArtifactError("runtime and model payload must be separate artifacts");
        }
        if (signature != expected_signature()) throw LocalModelArtifactError("model detached signature does not match manifest");
    }

    void verify_payload(std::uint64_t observed_size, const std::string& observed_sha256) const {
        validate();
        if (observed_size != size_bytes || observed_sha256 != sha256) throw LocalModelArtifactError("model payload hash or size mismatch");
    }

    std::string to_json() const {
        validate();
        std::ostringstream output;
        output << "{\"backend_compatibility\":" << local_model_json_string(backend_compatibility)
               << ",\"format\":" << local_model_json_string(format)
               << ",\"language\":" << local_model_json_string(language)
               << ",\"license_compatible\":" << local_model_json_bool(license_compatible)
               << ",\"license_id\":" << local_model_json_string(license_id)
               << ",\"model_id\":" << local_model_json_string(model_id)
               << ",\"payload_artifact_id\":" << local_model_json_string(payload_artifact_id)
               << ",\"payload_separate\":" << local_model_json_bool(payload_separate)
               << ",\"quantization\":" << local_model_json_string(quantization)
               << ",\"sha256\":" << local_model_json_string(sha256)
               << ",\"signature\":" << local_model_json_string(signature)
               << ",\"signature_algorithm\":" << local_model_json_string(signature_algorithm)
               << ",\"signing_key_id\":" << local_model_json_string(signing_key_id)
               << ",\"size_bytes\":" << size_bytes
               << ",\"runtime_artifact_id\":" << local_model_json_string(runtime_artifact_id) << '}';
        return output.str();
    }
};

class LocalModelBackend {
public:
    virtual ~LocalModelBackend() = default;
    virtual const std::string& backend_id() const = 0;
    virtual void load(const LocalModelArtifact& artifact) = 0;
    virtual LocalModelRawOutput invoke(const LocalModelRequest& request) = 0;
    virtual void cancel(const std::string& request_id) = 0;
    virtual void unload(const std::string& model_id) = 0;
};

struct LocalModelGatewayConfig {
    std::string backend_id;
    LocalModelSchedulingPolicy scheduling_policy{LocalModelSchedulingPolicy::priority_single_worker_v1};
    bool unload_after_request{true};

    void validate() const {
        local_model_required(backend_id, "backend_id");
    }
};

class LocalModelGateway {
public:
    LocalModelGateway(std::map<std::string, LocalModelBackend*> backends,
                      LocalModelGatewayConfig config,
                      std::optional<LocalModelArtifact> artifact)
        : backends_(std::move(backends)), config_(std::move(config)), artifact_(std::move(artifact)) {
        config_.validate();
        if (backends_.empty() || !backends_.contains(config_.backend_id)) throw LocalModelGatewayError("configured backend is unavailable");
        for (const auto& [id, backend] : backends_) {
            if (backend == nullptr || id != backend->backend_id()) throw LocalModelGatewayError("backend mapping key must match backend_id");
        }
        if (artifact_) artifact_->validate();
        worker_ = std::thread([this] { worker_loop(); });
    }

    LocalModelGateway(const LocalModelGateway&) = delete;
    LocalModelGateway& operator=(const LocalModelGateway&) = delete;

    ~LocalModelGateway() { close(); }

    std::future<LocalModelResponse> submit(LocalModelRequest request) {
        request.validate();
        if (!artifact_) throw LocalModelGatewayError("model is unavailable; dialogue is disabled");
        if (request.backend_id != config_.backend_id) throw LocalModelGatewayError("request backend does not match configured backend");
        if (request.model_id != artifact_->model_id) throw LocalModelGatewayError("request model does not match loaded artifact");
        auto job = std::make_shared<Job>();
        job->request = std::move(request);
        auto future = job->promise.get_future();
        {
            std::lock_guard lock(mutex_);
            if (closed_) throw LocalModelGatewayError("gateway is closed");
            if (jobs_.contains(job->request.request_id)) throw LocalModelGatewayError("request_id is already queued or active");
            job->sequence = sequence_++;
            jobs_[job->request.request_id] = job;
            queue_.push_back(job);
            audit_.push_back({"queued", job->request.request_id});
        }
        condition_.notify_one();
        return future;
    }

    LocalModelResponse invoke(LocalModelRequest request) { return submit(std::move(request)).get(); }

    bool cancel(const std::string& request_id) {
        std::shared_ptr<Job> job;
        LocalModelBackend* backend = nullptr;
        {
            std::lock_guard lock(mutex_);
            const auto found = jobs_.find(request_id);
            if (found == jobs_.end() || found->second->finished) return false;
            job = found->second;
            job->cancelled = true;
            ++cancellation_count_;
            audit_.push_back({"cancel_requested", request_id});
            if (active_.has_value() && *active_ == job) backend = backends_.at(job->request.backend_id);
        }
        if (backend != nullptr) backend->cancel(request_id);
        condition_.notify_one();
        return true;
    }

    void configure_backend(const std::string& backend_id) {
        local_model_required(backend_id, "backend_id");
        std::lock_guard lock(mutex_);
        if (!queue_.empty() || active_.has_value()) throw LocalModelGatewayError("cannot change backend while work is pending");
        if (!backends_.contains(backend_id)) throw LocalModelGatewayError("configured backend is unavailable");
        if (loaded_model_) unload_current_unlocked(backends_.at(config_.backend_id));
        config_.backend_id = backend_id;
        audit_.push_back({"backend_configured", backend_id});
    }

    void close() {
        {
            std::lock_guard lock(mutex_);
            if (closed_) return;
            closed_ = true;
        }
        condition_.notify_all();
        if (worker_.joinable()) worker_.join();
        unload_current();
    }

    const LocalModelGatewayConfig& config() const { return config_; }
    const std::optional<LocalModelArtifact>& artifact() const { return artifact_; }

    std::string metrics_json() const {
        std::lock_guard lock(mutex_);
        std::ostringstream output;
        output << "{\"ablation\":" << local_model_json_string(LOCAL_MODEL_ABLATION)
               << ",\"active_request_id\":" << (active_ ? local_model_json_string((*active_)->request.request_id) : "null")
               << ",\"baseline_scheduler_id\":" << local_model_json_string(LOCAL_MODEL_BASELINE_SCHEDULER_ID)
               << ",\"cancellation_count\":" << cancellation_count_
               << ",\"completed_count\":" << completed_count_
               << ",\"falsification\":" << local_model_json_string(LOCAL_MODEL_FALSIFICATION)
               << ",\"hypothesis\":" << local_model_json_string(LOCAL_MODEL_HYPOTHESIS)
               << ",\"invalid_response_count\":" << invalid_response_count_
               << ",\"loaded_model_id\":" << (loaded_model_ ? local_model_json_string(*loaded_model_) : "null")
               << ",\"max_concurrent_inferences\":" << max_concurrent_inferences_
               << ",\"max_loaded_models\":" << max_loaded_models_
               << ",\"model_available\":" << local_model_json_bool(artifact_.has_value())
               << ",\"queued_request_count\":" << queue_.size()
               << ",\"scheduler_id\":" << local_model_json_string(local_model_policy_string(config_.scheduling_policy))
               << ",\"timeout_count\":" << timeout_count_ << '}';
        return output.str();
    }

    std::string snapshot_json() const {
        std::lock_guard lock(mutex_);
        std::ostringstream output;
        output << "{\"audit\":[";
        for (std::size_t index = 0; index < audit_.size(); ++index) {
            if (index) output << ',';
            output << "{\"event\":" << local_model_json_string(audit_[index].first)
                   << ",\"request_id\":" << local_model_json_string(audit_[index].second) << '}';
        }
        output << "],\"config\":{\"backend_id\":" << local_model_json_string(config_.backend_id)
               << ",\"scheduling_policy\":" << local_model_json_string(local_model_policy_string(config_.scheduling_policy))
               << ",\"unload_after_request\":" << local_model_json_bool(config_.unload_after_request) << "}"
               << ",\"metrics\":" << metrics_json_unlocked() << '}';
        return output.str();
    }

private:
    struct Job {
        LocalModelRequest request;
        std::promise<LocalModelResponse> promise;
        std::uint64_t sequence{0};
        bool cancelled{false};
        bool finished{false};
    };

    void worker_loop() {
        while (true) {
            std::shared_ptr<Job> job;
            {
                std::unique_lock lock(mutex_);
                condition_.wait(lock, [&] { return closed_ || !queue_.empty(); });
                if (closed_ && queue_.empty()) return;
                auto selected = queue_.begin();
                for (auto iterator = queue_.begin() + 1; iterator != queue_.end(); ++iterator) {
                    if (better(*iterator, *selected)) selected = iterator;
                }
                job = *selected;
                queue_.erase(selected);
                active_ = job;
            }
            try {
                if (job->cancelled) throw LocalModelCancelledError("request cancelled before inference");
                auto response = execute(*job);
                if (job->cancelled) throw LocalModelCancelledError("request cancelled during inference");
                {
                    std::lock_guard lock(mutex_);
                    ++completed_count_;
                    audit_.push_back({"completed", job->request.request_id});
                    job->finished = true;
                    jobs_.erase(job->request.request_id);
                    active_.reset();
                }
                job->promise.set_value(std::move(response));
            } catch (...) {
                const auto exception = std::current_exception();
                {
                    std::lock_guard lock(mutex_);
                    std::string event = "failed";
                    try { if (exception) std::rethrow_exception(exception); }
                    catch (const LocalModelTimeoutError&) { event = "timeout"; }
                    catch (const LocalModelCancelledError&) { event = "cancelled"; }
                    catch (const InvalidLocalModelResponseError&) { event = "invalid_response"; }
                    catch (...) {}
                    audit_.push_back({event, job->request.request_id});
                    job->finished = true;
                    jobs_.erase(job->request.request_id);
                    active_.reset();
                }
                try { job->promise.set_exception(exception); } catch (...) {}
            }
        }
    }

    bool better(const std::shared_ptr<Job>& left, const std::shared_ptr<Job>& right) const {
        if (config_.scheduling_policy == LocalModelSchedulingPolicy::priority_single_worker_v1 && left->request.priority != right->request.priority) {
            return left->request.priority > right->request.priority;
        }
        return left->sequence < right->sequence;
    }

    LocalModelResponse execute(Job& job) {
        LocalModelBackend* backend = backends_.at(job.request.backend_id);
        load_current(backend, job.request.model_id);
        const auto started = std::chrono::steady_clock::now();
        {
            std::lock_guard lock(mutex_);
            ++concurrent_inferences_;
            max_concurrent_inferences_ = std::max(max_concurrent_inferences_, concurrent_inferences_);
        }
        try {
            std::packaged_task<LocalModelRawOutput()> task([backend, request = job.request] { return backend->invoke(request); });
            auto future = task.get_future();
            std::thread(std::move(task)).detach();
            if (future.wait_for(std::chrono::duration<double>(job.request.timeout_seconds)) != std::future_status::ready) {
                backend->cancel(job.request.request_id);
                {
                    std::lock_guard lock(mutex_);
                    ++timeout_count_;
                }
                future.wait();
                throw LocalModelTimeoutError("local model request timed out");
            }
            const auto raw = future.get();
            if (job.cancelled) throw LocalModelCancelledError("local model request cancelled");
            if (raw.kind.empty()) {
                std::lock_guard lock(mutex_);
                ++invalid_response_count_;
                throw InvalidLocalModelResponseError("backend output must contain a kind");
            }
            const auto elapsed = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
            LocalModelResponse response{
                digest::uuid5(LOCAL_MODEL_NAMESPACE, job.request.request_id + ":" + job.request.backend_id + ":" + job.request.model_id + ":" + raw.kind),
                job.request.request_id, job.request.backend_id, job.request.model_id, raw.kind, raw.fields, std::max(0.0, elapsed)};
            response.validate();
            {
                std::lock_guard lock(mutex_);
                --concurrent_inferences_;
                if (config_.unload_after_request) unload_current_unlocked(backend);
            }
            return response;
        } catch (...) {
            std::lock_guard lock(mutex_);
            --concurrent_inferences_;
            if (config_.unload_after_request) unload_current_unlocked(backend);
            throw;
        }
        std::lock_guard lock(mutex_);
        --concurrent_inferences_;
        if (config_.unload_after_request) unload_current_unlocked(backend);
        throw LocalModelGatewayError("unreachable local model execution path");
    }

    void load_current(LocalModelBackend* backend, const std::string& model_id) {
        std::lock_guard lock(mutex_);
        if (loaded_model_ && *loaded_model_ == model_id) return;
        if (loaded_model_) unload_current_unlocked(backend);
        backend->load(*artifact_);
        loaded_model_ = model_id;
        max_loaded_models_ = std::max<std::size_t>(max_loaded_models_, 1);
        audit_.push_back({"loaded", model_id});
    }

    void unload_current_unlocked(LocalModelBackend* backend) {
        if (!loaded_model_) return;
        const auto model_id = *loaded_model_;
        try { backend->unload(model_id); }
        catch (...) { audit_.push_back({"unload_failed", model_id}); loaded_model_.reset(); throw; }
        audit_.push_back({"unloaded", model_id});
        loaded_model_.reset();
    }

    void unload_current() {
        std::lock_guard lock(mutex_);
        if (loaded_model_) unload_current_unlocked(backends_.at(config_.backend_id));
    }

    std::string metrics_json_unlocked() const {
        std::ostringstream output;
        output << "{\"ablation\":" << local_model_json_string(LOCAL_MODEL_ABLATION)
               << ",\"active_request_id\":" << (active_ ? local_model_json_string((*active_)->request.request_id) : "null")
               << ",\"baseline_scheduler_id\":" << local_model_json_string(LOCAL_MODEL_BASELINE_SCHEDULER_ID)
               << ",\"cancellation_count\":" << cancellation_count_
               << ",\"completed_count\":" << completed_count_
               << ",\"falsification\":" << local_model_json_string(LOCAL_MODEL_FALSIFICATION)
               << ",\"hypothesis\":" << local_model_json_string(LOCAL_MODEL_HYPOTHESIS)
               << ",\"invalid_response_count\":" << invalid_response_count_
               << ",\"loaded_model_id\":" << (loaded_model_ ? local_model_json_string(*loaded_model_) : "null")
               << ",\"max_concurrent_inferences\":" << max_concurrent_inferences_
               << ",\"max_loaded_models\":" << max_loaded_models_
               << ",\"model_available\":" << local_model_json_bool(artifact_.has_value())
               << ",\"queued_request_count\":" << queue_.size()
               << ",\"scheduler_id\":" << local_model_json_string(local_model_policy_string(config_.scheduling_policy))
               << ",\"timeout_count\":" << timeout_count_ << '}';
        return output.str();
    }

    std::map<std::string, LocalModelBackend*> backends_;
    LocalModelGatewayConfig config_;
    std::optional<LocalModelArtifact> artifact_;
    mutable std::mutex mutex_;
    std::condition_variable condition_;
    std::thread worker_;
    bool closed_{false};
    std::uint64_t sequence_{0};
    std::vector<std::shared_ptr<Job>> queue_;
    std::map<std::string, std::shared_ptr<Job>> jobs_;
    std::optional<std::shared_ptr<Job>> active_;
    std::optional<std::string> loaded_model_;
    std::vector<std::pair<std::string, std::string>> audit_;
    std::size_t concurrent_inferences_{0};
    std::size_t max_concurrent_inferences_{0};
    std::size_t max_loaded_models_{0};
    std::size_t completed_count_{0};
    std::size_t timeout_count_{0};
    std::size_t cancellation_count_{0};
    std::size_t invalid_response_count_{0};
};

struct LocalModelAvailability {
    bool model_available{false};
    bool dialogue_enabled{false};
    bool timeline_available{true};
    bool privacy_available{true};
    bool diagnostics_available{true};
    std::string reason_code{"model_absent"};

    std::string to_json() const {
        std::ostringstream output;
        output << "{\"diagnostics_available\":" << local_model_json_bool(diagnostics_available)
               << ",\"dialogue_enabled\":" << local_model_json_bool(dialogue_enabled)
               << ",\"model_available\":" << local_model_json_bool(model_available)
               << ",\"privacy_available\":" << local_model_json_bool(privacy_available)
               << ",\"reason_code\":" << local_model_json_string(reason_code)
               << ",\"timeline_available\":" << local_model_json_bool(timeline_available) << '}';
        return output.str();
    }
};

class LocalModelGatewayPlugin final : public CapabilityPlugin {
public:
    LocalModelGatewayPlugin() {
        descriptor_.capability_id = "inference.local_model_gateway";
        descriptor_.implementation_id = "native.local_model_gateway";
        descriptor_.implementation_version = "1.0.0";
        descriptor_.kind = "optional_inference_service";
        descriptor_.provides.push_back({"infer.local_model", "urn:eu-digital:local-model-response:1"});
        descriptor_.supports_hot_plug = true;
        descriptor_.supports_checkpoint = false;
    }

    const CapabilityDescriptor& descriptor() const override { return descriptor_; }
    void validate_manifest() override {}
    void configure() override {}
    void initialize() override {}
    void calibrate() override {}
    bool health_check() override { return true; }
    void start() override {}
    void drain() override {}
    std::map<std::string, std::string> checkpoint() override { return {}; }
    void stop() override {}
    void uninstall() override {}

private:
    CapabilityDescriptor descriptor_;
};

}  // namespace eu_digital
