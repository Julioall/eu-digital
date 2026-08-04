#include "core/adapters/ollama_model_backend.hpp"

#include <cassert>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <future>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using namespace eu_digital;

OllamaBackendConfig config(bool enabled = true) {
  OllamaBackendConfig value;
  value.enabled = enabled;
  value.validate();
  return value;
}

OllamaModelBinding binding() {
  OllamaModelBinding value;
  value.ollama_model = "qwen3-vl:2b";
  value.ollama_digest =
      "0635d9d857d497aeadba3d7d27485746c50554446f9f6ec01ef39788221adbe8";
  value.ollama_size_bytes = 1889519687;
  value.artifact.model_id = value.ollama_model;
  value.artifact.quantization = "Q4_K_M";
  value.artifact.size_bytes = 1889496384;
  value.artifact.sha256 =
      "ebabfa59b71a5b96e0281ec2994977e785284e0939807a99fc340dec3c6f10de";
  value.artifact.language = "pt-multilingual";
  value.artifact.license_id = "Apache-2.0";
  value.artifact.license_compatible = true;
  value.artifact.backend_compatibility = OLLAMA_BACKEND_COMPATIBILITY;
  value.artifact.signing_key_id = "local-manifest-digest-v1";
  value.artifact.runtime_artifact_id = "ollama-runtime-0.32.5-windows";
  value.artifact.payload_artifact_id = "qwen3-vl-2b-ebabfa59";
  value.artifact.signature = value.artifact.expected_signature();
  value.validate();
  return value;
}

std::string tags_json(std::string digest = binding().ollama_digest,
                      std::uint64_t size = binding().ollama_size_bytes,
                      std::string quantization = "Q4_K_M") {
  return std::string("{\"models\":[{") + "\"name\":\"qwen3-vl:2b\"," +
         "\"model\":\"qwen3-vl:2b\"," + "\"digest\":\"" + digest + "\"," +
         "\"size\":" + std::to_string(size) + "," +
         "\"details\":{\"format\":\"gguf\",\"quantization_level\":\"" +
         quantization + "\",\"family\":\"qwen3vl\"}," +
         "\"unknown_documented_field\":true}]}";
}

LocalModelRequest request(std::string id = "request-1",
                          double timeout_seconds = 1.0,
                          std::string prompt = "line\n\"quoted\"") {
  ModelPromptTemplate prompt_template{
      "dialogue", "1.0.0", "{content}", {"content"}};
  return {std::move(id),   "ollama",        "qwen3-vl:2b",    0,
          timeout_seconds, prompt_template, std::move(prompt)};
}

class MockTransport final : public IOllamaTransport {
public:
  enum class Failure { none, timeout, protocol };

  OllamaHttpResponse perform(const OllamaHttpRequest &request,
                             const std::string &request_id) override {
    request.validate();
    std::unique_lock lock(mutex_);
    requests_.push_back(request);
    request_ids_.push_back(request_id);
    entered_ = true;
    condition_.notify_all();
    if (request.path == "/api/generate" && block_generation_) {
      condition_.wait(lock, [&] { return cancelled_; });
      throw LocalModelCancelledError("mock cancelled");
    }
    if (responses_.empty())
      throw OllamaProtocolError("missing mock response");
    auto response = std::move(responses_.front());
    responses_.pop_front();
    const auto failure = failures_.empty() ? Failure::none : failures_.front();
    if (!failures_.empty())
      failures_.pop_front();
    lock.unlock();
    if (failure == Failure::timeout) {
      throw LocalModelTimeoutError("mock timeout");
    }
    if (failure == Failure::protocol) {
      throw OllamaProtocolError("mock protocol failure");
    }
    if (response.body.size() > request.max_response_bytes) {
      throw OllamaProtocolError("mock body limit");
    }
    return response;
  }

  void cancel(const std::string &request_id) override {
    std::lock_guard lock(mutex_);
    cancelled_ = true;
    cancelled_id_ = request_id;
    condition_.notify_all();
  }

  void push(OllamaHttpResponse response, Failure failure = Failure::none) {
    std::lock_guard lock(mutex_);
    responses_.push_back(std::move(response));
    failures_.push_back(failure);
  }

  void block_generation() {
    std::lock_guard lock(mutex_);
    block_generation_ = true;
    entered_ = false;
  }

  void wait_entered() {
    std::unique_lock lock(mutex_);
    condition_.wait(lock, [&] { return entered_; });
  }

  std::vector<OllamaHttpRequest> requests() const {
    std::lock_guard lock(mutex_);
    return requests_;
  }

  std::string cancelled_id() const {
    std::lock_guard lock(mutex_);
    return cancelled_id_;
  }

private:
  mutable std::mutex mutex_;
  std::condition_variable condition_;
  std::deque<OllamaHttpResponse> responses_;
  std::deque<Failure> failures_;
  std::vector<OllamaHttpRequest> requests_;
  std::vector<std::string> request_ids_;
  bool block_generation_{false};
  bool entered_{false};
  bool cancelled_{false};
  std::string cancelled_id_;
};

template <typename Error, typename Function> void rejects(Function &&function) {
  bool rejected = false;
  try {
    function();
  } catch (const Error &) {
    rejected = true;
  }
  assert(rejected);
}

std::shared_ptr<MockTransport>
loaded_backend(std::unique_ptr<OllamaModelBackend> &backend) {
  auto transport = std::make_shared<MockTransport>();
  transport->push({200, tags_json()});
  backend =
      std::make_unique<OllamaModelBackend>(transport, config(), binding());
  backend->load(binding().artifact);
  return transport;
}

} // namespace

int main() {
  using namespace eu_digital;

  const std::string config_json = R"({
      "schema_version":"1.0","enabled":true,"host":"127.0.0.1","port":11434,
      "connect_timeout_ms":2000,"send_timeout_ms":5000,
      "receive_timeout_ms":120000,"max_output_tokens":512,"max_response_bytes":1048576,
      "max_tags_bytes":4194304})";
  const auto parsed_config = OllamaBackendConfig::from_json(config_json);
  assert(parsed_config.enabled);
  assert(parsed_config.host == "127.0.0.1");
  rejects<OllamaConfigurationError>([&] {
    OllamaBackendConfig::from_json(
        config_json.substr(0, config_json.size() - 1) + ",\"extra\":1}");
  });
  rejects<OllamaConfigurationError>([&] {
    auto remote = parsed_config;
    remote.host = "localhost";
    remote.validate();
  });
  rejects<OllamaConfigurationError>([&] {
    auto remote = parsed_config;
    remote.port = 443;
    remote.validate();
  });

  const auto model_binding = binding();
  const auto parsed_binding = OllamaModelBinding::from_json(
      std::string("{\"schema_version\":\"1.0\",") +
      "\"ollama_model\":\"qwen3-vl:2b\"," + "\"ollama_digest\":\"" +
      model_binding.ollama_digest + "\"," +
      "\"ollama_size_bytes\":1889519687," +
      "\"artifact\":" + model_binding.artifact.to_json() + "}");
  assert(parsed_binding.artifact.sha256 == model_binding.artifact.sha256);
  rejects<OllamaConfigurationError>([&] {
    auto mismatched = model_binding;
    mismatched.ollama_model = "cloud-model";
    mismatched.validate();
  });

  OllamaHttpRequest forbidden{
      OllamaHttpMethod::post, "/api/pull", "{}", 1, 1, 1, 1};
  rejects<OllamaProtocolError>([&] { forbidden.validate(); });

  std::unique_ptr<OllamaModelBackend> backend;
  auto transport = loaded_backend(backend);
  const auto load_requests = transport->requests();
  assert(load_requests.size() == 1);
  assert(load_requests[0].method == OllamaHttpMethod::get);
  assert(load_requests[0].path == "/api/tags");
  assert(load_requests[0].body.empty());

  transport->push(
      {200, "{\"model\":\"qwen3-vl:2b\",\"response\":\"ol\\u00e1\\nlocal "
            "\\uD83D\\uDE00\"," +
                std::string("\"done\":true,\"total_duration\":42}")});
  const auto output = backend->invoke(request());
  assert(output.kind == "generation");
  assert(output.fields.at("text") == "ol\xC3\xA1\nlocal \xF0\x9F\x98\x80");
  const auto invoke_requests = transport->requests();
  assert(invoke_requests.size() == 2);
  assert(invoke_requests[1].method == OllamaHttpMethod::post);
  assert(invoke_requests[1].path == "/api/generate");
  assert(invoke_requests[1].body ==
         "{\"keep_alive\":0,\"model\":\"qwen3-vl:2b\"," +
             std::string(
                 "\"prompt\":\"line\\n\\\"quoted\\\"\",\"stream\":"
                 "false,\"think\":false,\"options\":{\"num_predict\":512}}"));
  assert(invoke_requests[1].receive_timeout_ms == 1000);

  {
    auto disabled_transport = std::make_shared<MockTransport>();
    OllamaModelBackend disabled(disabled_transport, config(false),
                                model_binding);
    rejects<OllamaConfigurationError>(
        [&] { disabled.load(model_binding.artifact); });
    assert(disabled_transport->requests().empty());
  }

  auto invalid_utf8 = std::string("{\"model\":\"qwen3-vl:2b\",\"response\":\"");
  invalid_utf8.push_back(static_cast<char>(0xff));
  invalid_utf8 += "\",\"done\":true}";
  for (const auto &invalid : std::vector<std::string>{
           "{\"model\":\"qwen3-vl:2b\",\"response\":\"x\",\"done\":true",
           "not-json",
           "{\"model\":\"qwen3-vl:2b\",\"response\":7,\"done\":true}",
           "{\"model\":\"qwen3-vl:2b\",\"response\":\"x\",\"done\":false}",
           "{\"model\":\"qwen3-vl:2b\",\"model\":\"other\",\"response\":\"x\","
           "\"done\":true}",
           "{\"model\":\"other\",\"response\":\"x\",\"done\":true}",
           "{\"model\":\"qwen3-vl:2b\",\"response\":\"\",\"done\":true}",
           "{\"model\":\"qwen3-vl:2b\",\"response\":\"\\uD800\",\"done\":true}",
           "{\"model\":\"qwen3-vl:2b\",\"response\":\"x\",\"done\":true} "
           "trailing",
           invalid_utf8}) {
    std::unique_ptr<OllamaModelBackend> invalid_backend;
    auto invalid_transport = loaded_backend(invalid_backend);
    invalid_transport->push({200, invalid});
    rejects<LocalModelGatewayError>(
        [&] { (void)invalid_backend->invoke(request("invalid")); });
  }

  {
    auto missing = std::make_shared<MockTransport>();
    missing->push({200, "{\"models\":[]}"});
    OllamaModelBackend missing_backend(missing, config(), model_binding);
    rejects<LocalModelArtifactError>(
        [&] { missing_backend.load(model_binding.artifact); });
    assert(missing->requests().size() == 1);
    assert(missing->requests()[0].path == "/api/tags");
  }
  {
    auto mismatch = std::make_shared<MockTransport>();
    mismatch->push({200, tags_json(std::string(64, 'a'))});
    OllamaModelBackend mismatch_backend(mismatch, config(), model_binding);
    rejects<LocalModelArtifactError>(
        [&] { mismatch_backend.load(model_binding.artifact); });
  }
  {
    auto malformed = std::make_shared<MockTransport>();
    malformed->push({200, "{\"models\":["});
    OllamaModelBackend malformed_backend(malformed, config(), model_binding);
    rejects<OllamaProtocolError>(
        [&] { malformed_backend.load(model_binding.artifact); });
  }
  {
    auto status = std::make_shared<MockTransport>();
    status->push({404, "{\"error\":\"model not found\"}"});
    OllamaModelBackend status_backend(status, config(), model_binding);
    rejects<OllamaProtocolError>(
        [&] { status_backend.load(model_binding.artifact); });
  }
  {
    std::unique_ptr<OllamaModelBackend> timeout_backend;
    auto timeout_transport = loaded_backend(timeout_backend);
    timeout_transport->push({200, ""}, MockTransport::Failure::timeout);
    rejects<LocalModelTimeoutError>(
        [&] { (void)timeout_backend->invoke(request("timeout", 0.02)); });
    assert(timeout_transport->requests().back().receive_timeout_ms == 20);
  }
  {
    std::unique_ptr<OllamaModelBackend> failed_backend;
    auto failed_transport = loaded_backend(failed_backend);
    failed_transport->push({200, ""}, MockTransport::Failure::protocol);
    rejects<OllamaProtocolError>(
        [&] { (void)failed_backend->invoke(request("failure")); });
  }
  {
    std::unique_ptr<OllamaModelBackend> cancelling_backend;
    auto cancelling_transport = loaded_backend(cancelling_backend);
    cancelling_transport->block_generation();
    auto future = std::async(std::launch::async, [&] {
      return cancelling_backend->invoke(request("cancel-me"));
    });
    cancelling_transport->wait_entered();
    cancelling_backend->cancel("cancel-me");
    rejects<LocalModelCancelledError>([&] { (void)future.get(); });
    assert(cancelling_transport->cancelled_id() == "cancel-me");
  }
  {
    auto limited_transport = std::make_shared<MockTransport>();
    limited_transport->push(
        {200, tags_json() + std::string(5 * 1024 * 1024, ' ')});
    OllamaModelBackend limited_backend(limited_transport, config(),
                                       model_binding);
    rejects<OllamaProtocolError>(
        [&] { limited_backend.load(model_binding.artifact); });
  }

  {
    auto gateway_transport = std::make_shared<MockTransport>();
    gateway_transport->push({200, tags_json()});
    gateway_transport->push({200, "{\"model\":\"qwen3-vl:2b\",\"response\":"
                                  "\"gateway ok\",\"done\":true}"});
    OllamaModelBackend gateway_backend(gateway_transport, config(),
                                       model_binding);
    LocalModelGateway gateway({{"ollama", &gateway_backend}}, {"ollama"},
                              model_binding.artifact);
    const auto response = gateway.invoke(request("gateway"));
    assert(response.output_kind == "generation");
    assert(response.output_fields.at("text") == "gateway ok");
    gateway.close();
  }

  {
    CapabilityRegistry registry;
    ModuleLifecycleManager lifecycle(registry);
    rejects<NoCapabilityProvider>(
        [&] { (void)registry.resolve("infer.local_model.backend"); });

    OllamaBackendPlugin broken(config(), model_binding, [] { return false; });
    assert(!lifecycle.install(broken));
    assert(registry.record(broken.descriptor().implementation_id).state.state ==
           CapabilityState::failed);
    lifecycle.remove(broken.descriptor().implementation_id);

    OllamaBackendPlugin plugin(config(), model_binding, [] { return true; });
    assert(lifecycle.install(plugin, 10));
    assert(registry.resolve("infer.local_model.backend").implementation_id ==
           plugin.descriptor().implementation_id);
    lifecycle.remove(plugin.descriptor().implementation_id);
    rejects<NoCapabilityProvider>(
        [&] { (void)registry.resolve("infer.local_model.backend"); });
    assert(lifecycle.install(plugin, 10));
    assert(registry.resolve("infer.local_model.backend").implementation_id ==
           plugin.descriptor().implementation_id);
    lifecycle.remove(plugin.descriptor().implementation_id);

    CapabilityDescriptor substitute;
    substitute.capability_id = "inference.backend.fixture";
    substitute.implementation_id = "fixture.ollama_substitute";
    substitute.implementation_version = "1.0.0";
    substitute.kind = "optional_inference_backend";
    substitute.provides.push_back(
        {"infer.local_model.backend", "urn:eu-digital:local-model-response:1"});
    registry.register_instance(substitute, std::make_shared<int>(1), 20);
    assert(registry.resolve("infer.local_model.backend").implementation_id ==
           substitute.implementation_id);
  }

  return 0;
}
