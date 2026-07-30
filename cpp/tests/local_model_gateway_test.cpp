#include "core/local_model_gateway.hpp"

#include <cassert>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <set>
#include <thread>

namespace {

class FixtureBackend final : public eu_digital::LocalModelBackend {
public:
    explicit FixtureBackend(std::string id) : id_(std::move(id)) {}

    const std::string& backend_id() const override { return id_; }

    void load(const eu_digital::LocalModelArtifact& artifact) override {
        std::lock_guard lock(mutex_);
        events_.push_back({"load", artifact.model_id});
    }

    eu_digital::LocalModelRawOutput invoke(const eu_digital::LocalModelRequest& request) override {
        {
            std::lock_guard lock(mutex_);
            events_.push_back({"invoke", request.request_id});
            ++in_flight_;
            max_in_flight_ = std::max(max_in_flight_, in_flight_);
            invoked_.insert(request.request_id);
        }
        invoked_condition_.notify_all();
        std::unique_lock lock(mutex_);
        release_condition_.wait(lock, [&] { return released_ || cancelled_.contains(request.request_id); });
        --in_flight_;
        if (invalid_output_) return {"", {}};
        return output_;
    }

    void cancel(const std::string& request_id) override {
        {
            std::lock_guard lock(mutex_);
            cancelled_.insert(request_id);
            released_ = true;
            events_.push_back({"cancel", request_id});
        }
        release_condition_.notify_all();
    }

    void unload(const std::string& model_id) override {
        std::lock_guard lock(mutex_);
        events_.push_back({"unload", model_id});
    }

    void wait_invoked(const std::string& request_id) {
        std::unique_lock lock(mutex_);
        invoked_condition_.wait(lock, [&] { return invoked_.contains(request_id); });
    }

    void release() {
        {
            std::lock_guard lock(mutex_);
            released_ = true;
        }
        release_condition_.notify_all();
    }

    void block() {
        std::lock_guard lock(mutex_);
        released_ = false;
    }

    bool invoked(const std::string& request_id) const {
        std::lock_guard lock(mutex_);
        return invoked_.contains(request_id);
    }

    std::vector<std::pair<std::string, std::string>> events() const {
        std::lock_guard lock(mutex_);
        return events_;
    }

    int max_in_flight() const {
        std::lock_guard lock(mutex_);
        return max_in_flight_;
    }

    bool invalid_output_{false};
    eu_digital::LocalModelRawOutput output_{"summary", {{"text", "fixture response"}}};

private:
    std::string id_;
    mutable std::mutex mutex_;
    std::condition_variable invoked_condition_;
    std::condition_variable release_condition_;
    std::vector<std::pair<std::string, std::string>> events_;
    std::set<std::string> invoked_;
    std::set<std::string> cancelled_;
    int in_flight_{0};
    int max_in_flight_{0};
    bool released_{true};
};

eu_digital::LocalModelArtifact artifact() {
    eu_digital::LocalModelArtifact value;
    value.model_id = "fixture-gguf-pt";
    value.quantization = "Q4_K_M";
    value.size_bytes = 1024 * 1024;
    value.sha256 = std::string(64, 'a');
    value.language = "pt-BR";
    value.license_id = "Apache-2.0";
    value.license_compatible = true;
    value.backend_compatibility = "fixture.native.cpu.v1";
    value.signing_key_id = "fixture-key";
    value.runtime_artifact_id = "eu-digital-runtime-0.1.0";
    value.payload_artifact_id = "eu-digital-model-fixture-q4.package";
    value.signature = value.expected_signature();
    value.validate();
    value.verify_payload(value.size_bytes, value.sha256);
    return value;
}

eu_digital::ModelPromptTemplate prompt_template() {
    return {"dialogue", "1.0.0", "Respond to: {content}", {"content"}};
}

eu_digital::LocalModelRequest request(const std::string& id, int priority = 0, double timeout = 1.0,
                                      const std::string& backend_id = "fixture") {
    const auto template_value = prompt_template();
    return {id, backend_id, "fixture-gguf-pt", priority, timeout, template_value,
            template_value.render({{"content", id}})};
}

bool has_event(const std::vector<std::pair<std::string, std::string>>& events,
               const std::string& event, const std::string& value) {
    return std::find(events.begin(), events.end(), std::pair{event, value}) != events.end();
}

}  // namespace

int main() {
    using namespace eu_digital;

    auto model = artifact();
    bool invalid_artifact = false;
    try {
        auto invalid = model;
        invalid.size_bytes = LOCAL_MODEL_MAX_BYTES + 1;
        invalid.validate();
    } catch (const LocalModelArtifactError&) {
        invalid_artifact = true;
    }
    assert(invalid_artifact);
    bool invalid_payload = false;
    try { model.verify_payload(model.size_bytes, std::string(64, 'b')); }
    catch (const LocalModelArtifactError&) { invalid_payload = true; }
    assert(invalid_payload);

    FixtureBackend unavailable_backend("fixture");
    {
        LocalModelGateway gateway({{"fixture", &unavailable_backend}}, {"fixture"}, std::nullopt);
        bool unavailable = false;
        try { gateway.invoke(request("absent")); }
        catch (const LocalModelGatewayError&) { unavailable = true; }
        assert(unavailable);
        gateway.close();
    }

    FixtureBackend priority_backend("fixture");
    priority_backend.block();
    {
        LocalModelGateway gateway({{"fixture", &priority_backend}}, {"fixture"}, model);
        auto first = gateway.submit(request("first", 0));
        priority_backend.wait_invoked("first");
        auto low = gateway.submit(request("low", 1));
        auto high = gateway.submit(request("high", 9));
        priority_backend.release();
        (void)first.get();
        (void)high.get();
        (void)low.get();
        gateway.close();
        const auto events = priority_backend.events();
        std::vector<std::string> invokes;
        for (const auto& event : events) if (event.first == "invoke") invokes.push_back(event.second);
        assert((invokes == std::vector<std::string>{"first", "high", "low"}));
        assert(priority_backend.max_in_flight() == 1);
    }

    FixtureBackend timeout_backend("fixture");
    timeout_backend.block();
    {
        LocalModelGateway gateway({{"fixture", &timeout_backend}}, {"fixture"}, model);
        auto timed = gateway.submit(request("timeout", 0, 0.02));
        timeout_backend.wait_invoked("timeout");
        bool timed_out = false;
        try { (void)timed.get(); }
        catch (const LocalModelTimeoutError&) { timed_out = true; }
        assert(timed_out);
        gateway.close();
        assert(gateway.metrics_json().find("\"timeout_count\":1") != std::string::npos);
        assert(has_event(timeout_backend.events(), "cancel", "timeout"));
        assert(has_event(timeout_backend.events(), "unload", "fixture-gguf-pt"));
    }

    FixtureBackend cancellation_backend("fixture");
    cancellation_backend.block();
    {
        LocalModelGateway gateway({{"fixture", &cancellation_backend}}, {"fixture"}, model);
        auto active = gateway.submit(request("active"));
        cancellation_backend.wait_invoked("active");
        auto queued = gateway.submit(request("queued"));
        assert(gateway.cancel("queued"));
        cancellation_backend.release();
        bool cancelled = false;
        try { (void)queued.get(); }
        catch (const LocalModelCancelledError&) { cancelled = true; }
        assert(cancelled);
        (void)active.get();
        gateway.close();
        assert(!cancellation_backend.invoked("queued"));
    }

    FixtureBackend invalid_backend("fixture");
    invalid_backend.invalid_output_ = true;
    {
        LocalModelGateway gateway({{"fixture", &invalid_backend}}, {"fixture"}, model);
        bool rejected = false;
        try { gateway.invoke(request("invalid")); }
        catch (const InvalidLocalModelResponseError&) { rejected = true; }
        assert(rejected);
        gateway.close();
    }

    FixtureBackend first_backend("first");
    FixtureBackend second_backend("second");
    {
        LocalModelGateway gateway({{"first", &first_backend}, {"second", &second_backend}}, {"first", LocalModelSchedulingPolicy::priority_single_worker_v1, false}, model);
        auto first = request("first-request", 0, 1.0, "first");
        (void)gateway.invoke(first);
        gateway.configure_backend("second");
        (void)gateway.invoke(request("second-request", 0, 1.0, "second"));
        assert(has_event(second_backend.events(), "invoke", "second-request"));
        assert(has_event(first_backend.events(), "unload", "fixture-gguf-pt"));
        gateway.close();
    }

    LocalModelGatewayPlugin plugin;
    assert(plugin.descriptor().capability_id == "inference.local_model_gateway");
    assert(plugin.descriptor().supports_hot_plug);
    assert(plugin.descriptor().provides_operation("infer.local_model"));
    return 0;
}
