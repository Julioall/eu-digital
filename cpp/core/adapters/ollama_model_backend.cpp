#include "core/adapters/ollama_model_backend.hpp"
#include "core/adapters/ollama_json.hpp"

#include <algorithm>
#include <limits>
#include <map>
#include <mutex>
#include <sstream>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <winhttp.h>
#endif

namespace eu_digital {
namespace {

namespace runtime_detail = ollama_json;

using runtime_detail::JsonValue;

[[noreturn]] void configuration_error(const std::string &message) {
  throw OllamaConfigurationError(message);
}

std::uint32_t bounded_u32(const JsonValue &value, const std::string &path,
                          std::uint64_t minimum, std::uint64_t maximum) {
  const auto parsed = runtime_detail::unsigned_number(value, path);
  if (parsed < minimum || parsed > maximum) {
    configuration_error(path + " is outside the permitted range");
  }
  return static_cast<std::uint32_t>(parsed);
}

std::uint64_t bounded_u64(const JsonValue &value, const std::string &path,
                          std::uint64_t minimum, std::uint64_t maximum) {
  const auto parsed = runtime_detail::unsigned_number(value, path);
  if (parsed < minimum || parsed > maximum) {
    configuration_error(path + " is outside the permitted range");
  }
  return parsed;
}

LocalModelArtifact parse_artifact(const JsonValue &value,
                                  const std::string &path) {
  const auto &object = runtime_detail::object(value, path);
  runtime_detail::exact_keys(
      object,
      {"model_id", "format", "quantization", "size_bytes", "sha256", "language",
       "license_id", "license_compatible", "backend_compatibility",
       "signature_algorithm", "signature", "signing_key_id",
       "runtime_artifact_id", "payload_artifact_id", "payload_separate"},
      path);
  LocalModelArtifact artifact;
  artifact.model_id = runtime_detail::string(
      runtime_detail::required(object, "model_id", path), path + ".model_id");
  artifact.format = runtime_detail::string(
      runtime_detail::required(object, "format", path), path + ".format");
  artifact.quantization = runtime_detail::string(
      runtime_detail::required(object, "quantization", path),
      path + ".quantization");
  artifact.size_bytes =
      bounded_u64(runtime_detail::required(object, "size_bytes", path),
                  path + ".size_bytes", 1, LOCAL_MODEL_MAX_BYTES);
  artifact.sha256 = runtime_detail::string(
      runtime_detail::required(object, "sha256", path), path + ".sha256");
  artifact.language = runtime_detail::string(
      runtime_detail::required(object, "language", path), path + ".language");
  artifact.license_id = runtime_detail::string(
      runtime_detail::required(object, "license_id", path),
      path + ".license_id");
  artifact.license_compatible = runtime_detail::boolean(
      runtime_detail::required(object, "license_compatible", path),
      path + ".license_compatible");
  artifact.backend_compatibility = runtime_detail::string(
      runtime_detail::required(object, "backend_compatibility", path),
      path + ".backend_compatibility");
  artifact.signature_algorithm = runtime_detail::string(
      runtime_detail::required(object, "signature_algorithm", path),
      path + ".signature_algorithm");
  artifact.signature = runtime_detail::string(
      runtime_detail::required(object, "signature", path), path + ".signature");
  artifact.signing_key_id = runtime_detail::string(
      runtime_detail::required(object, "signing_key_id", path),
      path + ".signing_key_id");
  artifact.runtime_artifact_id = runtime_detail::string(
      runtime_detail::required(object, "runtime_artifact_id", path),
      path + ".runtime_artifact_id");
  artifact.payload_artifact_id = runtime_detail::string(
      runtime_detail::required(object, "payload_artifact_id", path),
      path + ".payload_artifact_id");
  artifact.payload_separate = runtime_detail::boolean(
      runtime_detail::required(object, "payload_separate", path),
      path + ".payload_separate");
  artifact.validate();
  return artifact;
}

std::uint32_t effective_receive_timeout(const OllamaBackendConfig &config,
                                        double request_seconds) {
  const auto requested = request_seconds * 1000.0;
  if (requested <= 0.0)
    return config.receive_timeout_ms;
  const auto clamped = std::min<double>(requested, config.receive_timeout_ms);
  return static_cast<std::uint32_t>(std::max(1.0, clamped));
}

} // namespace

void OllamaBackendConfig::validate() const {
  if (schema_version != OLLAMA_BACKEND_SCHEMA_VERSION) {
    configuration_error("unsupported Ollama backend schema version");
  }
  if (host != "127.0.0.1" || port != 11434) {
    configuration_error("Ollama endpoint must be 127.0.0.1:11434");
  }
  if (connect_timeout_ms < 100 || connect_timeout_ms > 10000 ||
      send_timeout_ms < 100 || send_timeout_ms > 30000 ||
      receive_timeout_ms < 100 || receive_timeout_ms > 300000) {
    configuration_error("Ollama timeout is outside the permitted range");
  }
  if (max_output_tokens < 8 || max_output_tokens > 1024) {
    configuration_error(
        "Ollama output token limit is outside the permitted range");
  }
  if (max_response_bytes < 1024 || max_response_bytes > 1024 * 1024 ||
      max_tags_bytes < 1024 || max_tags_bytes > 4 * 1024 * 1024) {
    configuration_error("Ollama body limit is outside the permitted range");
  }
}

OllamaBackendConfig OllamaBackendConfig::from_json(const std::string &json) {
  try {
    const auto root_value = runtime_detail::JsonParser(json).parse();
    const auto &root =
        runtime_detail::object(root_value, "OllamaBackendConfig");
    runtime_detail::exact_keys(root,
                               {"schema_version", "enabled", "host", "port",
                                "connect_timeout_ms", "send_timeout_ms",
                                "receive_timeout_ms", "max_output_tokens",
                                "max_response_bytes", "max_tags_bytes"},
                               "OllamaBackendConfig");
    OllamaBackendConfig config;
    config.schema_version = runtime_detail::string(
        runtime_detail::required(root, "schema_version", "OllamaBackendConfig"),
        "OllamaBackendConfig.schema_version");
    config.enabled = runtime_detail::boolean(
        runtime_detail::required(root, "enabled", "OllamaBackendConfig"),
        "OllamaBackendConfig.enabled");
    config.host = runtime_detail::string(
        runtime_detail::required(root, "host", "OllamaBackendConfig"),
        "OllamaBackendConfig.host");
    config.port = static_cast<std::uint16_t>(bounded_u32(
        runtime_detail::required(root, "port", "OllamaBackendConfig"),
        "OllamaBackendConfig.port", 1, 65535));
    config.connect_timeout_ms =
        bounded_u32(runtime_detail::required(root, "connect_timeout_ms",
                                             "OllamaBackendConfig"),
                    "OllamaBackendConfig.connect_timeout_ms", 100, 10000);
    config.send_timeout_ms =
        bounded_u32(runtime_detail::required(root, "send_timeout_ms",
                                             "OllamaBackendConfig"),
                    "OllamaBackendConfig.send_timeout_ms", 100, 30000);
    config.receive_timeout_ms =
        bounded_u32(runtime_detail::required(root, "receive_timeout_ms",
                                             "OllamaBackendConfig"),
                    "OllamaBackendConfig.receive_timeout_ms", 100, 300000);
    config.max_output_tokens =
        bounded_u32(runtime_detail::required(root, "max_output_tokens",
                                             "OllamaBackendConfig"),
                    "OllamaBackendConfig.max_output_tokens", 8, 1024);
    config.max_response_bytes = static_cast<std::size_t>(bounded_u64(
        runtime_detail::required(root, "max_response_bytes",
                                 "OllamaBackendConfig"),
        "OllamaBackendConfig.max_response_bytes", 1024, 1024 * 1024));
    config.max_tags_bytes = static_cast<std::size_t>(bounded_u64(
        runtime_detail::required(root, "max_tags_bytes", "OllamaBackendConfig"),
        "OllamaBackendConfig.max_tags_bytes", 1024, 4 * 1024 * 1024));
    config.validate();
    return config;
  } catch (const OllamaConfigurationError &) {
    throw;
  } catch (const std::exception &error) {
    configuration_error(std::string("invalid Ollama backend configuration: ") +
                        error.what());
  }
}

void OllamaModelBinding::validate() const {
  if (schema_version != OLLAMA_BACKEND_SCHEMA_VERSION) {
    configuration_error("unsupported Ollama model binding schema version");
  }
  local_model_required(ollama_model, "ollama_model");
  if (!local_model_hex_digest(ollama_digest)) {
    configuration_error("Ollama catalog digest is invalid");
  }
  if (ollama_size_bytes == 0 || ollama_size_bytes > LOCAL_MODEL_MAX_BYTES) {
    configuration_error("Ollama catalog size exceeds the 4 GiB policy");
  }
  artifact.validate();
  if (artifact.model_id != ollama_model ||
      artifact.backend_compatibility != OLLAMA_BACKEND_COMPATIBILITY) {
    configuration_error(
        "Ollama binding does not match its local model artifact");
  }
}

OllamaModelBinding OllamaModelBinding::from_json(const std::string &json) {
  try {
    const auto root_value = runtime_detail::JsonParser(json).parse();
    const auto &root = runtime_detail::object(root_value, "OllamaModelBinding");
    runtime_detail::exact_keys(root,
                               {"schema_version", "ollama_model",
                                "ollama_digest", "ollama_size_bytes",
                                "artifact"},
                               "OllamaModelBinding");
    OllamaModelBinding binding;
    binding.schema_version = runtime_detail::string(
        runtime_detail::required(root, "schema_version", "OllamaModelBinding"),
        "OllamaModelBinding.schema_version");
    binding.ollama_model = runtime_detail::string(
        runtime_detail::required(root, "ollama_model", "OllamaModelBinding"),
        "OllamaModelBinding.ollama_model");
    binding.ollama_digest = runtime_detail::string(
        runtime_detail::required(root, "ollama_digest", "OllamaModelBinding"),
        "OllamaModelBinding.ollama_digest");
    binding.ollama_size_bytes = bounded_u64(
        runtime_detail::required(root, "ollama_size_bytes",
                                 "OllamaModelBinding"),
        "OllamaModelBinding.ollama_size_bytes", 1, LOCAL_MODEL_MAX_BYTES);
    binding.artifact = parse_artifact(
        runtime_detail::required(root, "artifact", "OllamaModelBinding"),
        "OllamaModelBinding.artifact");
    binding.validate();
    return binding;
  } catch (const OllamaConfigurationError &) {
    throw;
  } catch (const std::exception &error) {
    configuration_error(std::string("invalid Ollama model binding: ") +
                        error.what());
  }
}

void OllamaHttpRequest::validate() const {
  const bool tags =
      method == OllamaHttpMethod::get && path == "/api/tags" && body.empty();
  const bool generate = method == OllamaHttpMethod::post &&
                        path == "/api/generate" && !body.empty();
  if (!tags && !generate) {
    throw OllamaProtocolError("Ollama endpoint or method is not permitted");
  }
  if (body.size() > 1024 * 1024) {
    throw OllamaProtocolError("Ollama request exceeds body limit");
  }
  if (connect_timeout_ms == 0 || send_timeout_ms == 0 ||
      receive_timeout_ms == 0 || max_response_bytes == 0) {
    throw OllamaProtocolError("Ollama request limits must be positive");
  }
}

#ifdef _WIN32

struct WinHttpOllamaTransport::Impl {
  struct ActiveRequest {
    std::mutex mutex;
    HINTERNET handle{nullptr};
    bool cancelled{false};
  };

  HINTERNET session{nullptr};
  HINTERNET connection{nullptr};
  std::mutex active_mutex;
  std::map<std::string, std::shared_ptr<ActiveRequest>> active;

  static bool cancelled(const std::shared_ptr<ActiveRequest> &state) {
    std::lock_guard lock(state->mutex);
    return state->cancelled;
  }

  static void close(const std::shared_ptr<ActiveRequest> &state) {
    std::lock_guard lock(state->mutex);
    if (state->handle != nullptr) {
      WinHttpCloseHandle(state->handle);
      state->handle = nullptr;
    }
  }

  void finish(const std::string &request_id,
              const std::shared_ptr<ActiveRequest> &state) {
    close(state);
    std::lock_guard lock(active_mutex);
    const auto found = active.find(request_id);
    if (found != active.end() && found->second == state)
      active.erase(found);
  }
};

#else

struct WinHttpOllamaTransport::Impl {};

#endif

WinHttpOllamaTransport::WinHttpOllamaTransport(
    const OllamaBackendConfig &config)
    : impl_(std::make_unique<Impl>()) {
  config.validate();
#ifdef _WIN32
  impl_->session = WinHttpOpen(
      L"EU-Digital Ollama Backend/1.0", WINHTTP_ACCESS_TYPE_NO_PROXY,
      WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
  if (impl_->session == nullptr) {
    throw OllamaProtocolError("cannot initialize local WinHTTP transport");
  }
  impl_->connection = WinHttpConnect(impl_->session, L"127.0.0.1", 11434, 0);
  if (impl_->connection == nullptr) {
    WinHttpCloseHandle(impl_->session);
    impl_->session = nullptr;
    throw OllamaProtocolError("cannot initialize local Ollama connection");
  }
#endif
}

WinHttpOllamaTransport::~WinHttpOllamaTransport() {
#ifdef _WIN32
  std::vector<std::shared_ptr<Impl::ActiveRequest>> requests;
  {
    std::lock_guard lock(impl_->active_mutex);
    for (const auto &[_, request] : impl_->active)
      requests.push_back(request);
    impl_->active.clear();
  }
  for (const auto &request : requests) {
    {
      std::lock_guard lock(request->mutex);
      request->cancelled = true;
    }
    Impl::close(request);
  }
  if (impl_->connection != nullptr)
    WinHttpCloseHandle(impl_->connection);
  if (impl_->session != nullptr)
    WinHttpCloseHandle(impl_->session);
#endif
}

OllamaHttpResponse
WinHttpOllamaTransport::perform(const OllamaHttpRequest &request,
                                const std::string &request_id) {
  request.validate();
  if (request_id.empty()) {
    throw OllamaProtocolError("Ollama request id must not be empty");
  }
#ifndef _WIN32
  (void)request;
  (void)request_id;
  throw OllamaProtocolError("WinHTTP Ollama transport requires Windows");
#else
  const std::wstring path(request.path.begin(), request.path.end());
  const wchar_t *method =
      request.method == OllamaHttpMethod::get ? L"GET" : L"POST";
  HINTERNET handle =
      WinHttpOpenRequest(impl_->connection, method, path.c_str(), nullptr,
                         WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
  if (handle == nullptr) {
    throw OllamaProtocolError("cannot create local Ollama request");
  }

  auto state = std::make_shared<Impl::ActiveRequest>();
  state->handle = handle;
  {
    std::lock_guard lock(impl_->active_mutex);
    if (impl_->active.contains(request_id)) {
      WinHttpCloseHandle(handle);
      throw OllamaProtocolError("duplicate active Ollama request id");
    }
    impl_->active.emplace(request_id, state);
  }

  const auto finish = [&] { impl_->finish(request_id, state); };
  const auto fail = [&](const std::string &operation) -> void {
    const auto error = GetLastError();
    const bool was_cancelled =
        Impl::cancelled(state) || error == ERROR_WINHTTP_OPERATION_CANCELLED;
    finish();
    if (was_cancelled)
      throw LocalModelCancelledError("local Ollama request cancelled");
    if (error == ERROR_WINHTTP_TIMEOUT) {
      throw LocalModelTimeoutError("local Ollama request timed out");
    }
    throw OllamaProtocolError("local Ollama " + operation + " failed");
  };

  if (!WinHttpSetTimeouts(handle, static_cast<int>(request.connect_timeout_ms),
                          static_cast<int>(request.connect_timeout_ms),
                          static_cast<int>(request.send_timeout_ms),
                          static_cast<int>(request.receive_timeout_ms))) {
    fail("timeout configuration");
  }
  DWORD disabled_features = WINHTTP_DISABLE_REDIRECTS;
  if (!WinHttpSetOption(handle, WINHTTP_OPTION_DISABLE_FEATURE,
                        &disabled_features, sizeof(disabled_features))) {
    fail("redirect protection");
  }

  const wchar_t *headers =
      request.method == OllamaHttpMethod::post
          ? L"Content-Type: application/json\r\nAccept: application/json\r\n"
          : L"Accept: application/json\r\n";
  void *body = request.body.empty() ? WINHTTP_NO_REQUEST_DATA
                                    : const_cast<char *>(request.body.data());
  const auto body_size = static_cast<DWORD>(request.body.size());
  if (!WinHttpSendRequest(handle, headers, static_cast<DWORD>(-1L), body,
                          body_size, body_size, 0)) {
    fail("send");
  }
  if (!WinHttpReceiveResponse(handle, nullptr))
    fail("receive");

  DWORD status = 0;
  DWORD status_size = sizeof(status);
  if (!WinHttpQueryHeaders(
          handle, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
          WINHTTP_HEADER_NAME_BY_INDEX, &status, &status_size,
          WINHTTP_NO_HEADER_INDEX)) {
    fail("status query");
  }

  std::string body_text;
  while (true) {
    DWORD available = 0;
    if (!WinHttpQueryDataAvailable(handle, &available))
      fail("body query");
    if (available == 0)
      break;
    if (body_text.size() + available > request.max_response_bytes) {
      finish();
      throw OllamaProtocolError("local Ollama response exceeds body limit");
    }
    std::vector<char> buffer(available);
    DWORD read = 0;
    if (!WinHttpReadData(handle, buffer.data(), available, &read))
      fail("body read");
    body_text.append(buffer.data(), read);
  }
  if (Impl::cancelled(state)) {
    finish();
    throw LocalModelCancelledError("local Ollama request cancelled");
  }
  finish();
  return {status, std::move(body_text)};
#endif
}

void WinHttpOllamaTransport::cancel(const std::string &request_id) {
#ifdef _WIN32
  std::shared_ptr<Impl::ActiveRequest> request;
  {
    std::lock_guard lock(impl_->active_mutex);
    const auto found = impl_->active.find(request_id);
    if (found == impl_->active.end())
      return;
    request = found->second;
  }
  {
    std::lock_guard lock(request->mutex);
    request->cancelled = true;
  }
  Impl::close(request);
#else
  (void)request_id;
#endif
}

OllamaModelBackend::OllamaModelBackend(OllamaBackendConfig config,
                                       OllamaModelBinding binding)
    : config_(std::move(config)), binding_(std::move(binding)) {
  config_.validate();
  binding_.validate();
  transport_ = std::make_shared<WinHttpOllamaTransport>(config_);
}

OllamaModelBackend::OllamaModelBackend(
    std::shared_ptr<IOllamaTransport> transport, OllamaBackendConfig config,
    OllamaModelBinding binding)
    : transport_(std::move(transport)), config_(std::move(config)),
      binding_(std::move(binding)) {
  if (!transport_)
    configuration_error("Ollama transport is required");
  config_.validate();
  binding_.validate();
}

const std::string &OllamaModelBackend::backend_id() const {
  return backend_id_;
}

OllamaHttpRequest
OllamaModelBackend::request(OllamaHttpMethod method, std::string path,
                            std::string body, std::uint32_t receive_timeout_ms,
                            std::size_t max_response_bytes) const {
  OllamaHttpRequest value{method,
                          std::move(path),
                          std::move(body),
                          config_.connect_timeout_ms,
                          config_.send_timeout_ms,
                          receive_timeout_ms,
                          max_response_bytes};
  value.validate();
  return value;
}

void OllamaModelBackend::load(const LocalModelArtifact &artifact) {
  if (!config_.enabled) {
    throw OllamaConfigurationError("Ollama backend is disabled");
  }
  artifact.validate();
  if (artifact.to_json() != binding_.artifact.to_json()) {
    throw LocalModelArtifactError(
        "model artifact does not match Ollama binding");
  }
  const auto response = transport_->perform(
      request(OllamaHttpMethod::get, "/api/tags", "",
              config_.receive_timeout_ms, config_.max_tags_bytes),
      "ollama-load:" + artifact.model_id);
  if (response.status_code != 200) {
    throw OllamaProtocolError("local Ollama catalog returned HTTP status " +
                              std::to_string(response.status_code));
  }
  verify_catalog(response.body);
  current_model_ = artifact.model_id;
}

void OllamaModelBackend::verify_catalog(const std::string &json) const {
  try {
    const auto root_value = runtime_detail::JsonParser(json).parse();
    const auto &root = runtime_detail::object(root_value, "OllamaTagsResponse");
    const auto &models = runtime_detail::array(
        runtime_detail::required(root, "models", "OllamaTagsResponse"),
        "OllamaTagsResponse.models");
    for (std::size_t index = 0; index < models.size(); ++index) {
      const auto path =
          "OllamaTagsResponse.models[" + std::to_string(index) + "]";
      const auto &model = runtime_detail::object(models[index], path);
      const auto name = runtime_detail::string(
          runtime_detail::required(model, "name", path), path + ".name");
      if (name != binding_.ollama_model)
        continue;
      const auto model_name = runtime_detail::string(
          runtime_detail::required(model, "model", path), path + ".model");
      const auto digest = runtime_detail::string(
          runtime_detail::required(model, "digest", path), path + ".digest");
      const auto size = runtime_detail::unsigned_number(
          runtime_detail::required(model, "size", path), path + ".size");
      const auto &details = runtime_detail::object(
          runtime_detail::required(model, "details", path), path + ".details");
      const auto format = runtime_detail::string(
          runtime_detail::required(details, "format", path + ".details"),
          path + ".details.format");
      const auto quantization = runtime_detail::string(
          runtime_detail::required(details, "quantization_level",
                                   path + ".details"),
          path + ".details.quantization_level");
      if (model_name != binding_.ollama_model ||
          digest != binding_.ollama_digest ||
          size != binding_.ollama_size_bytes || format != "gguf" ||
          quantization != binding_.artifact.quantization) {
        throw LocalModelArtifactError(
            "installed Ollama model does not match its binding");
      }
      return;
    }
    throw LocalModelArtifactError("bound Ollama model is not installed");
  } catch (const LocalModelArtifactError &) {
    throw;
  } catch (const std::exception &error) {
    throw OllamaProtocolError(
        std::string("invalid local Ollama catalog response: ") + error.what());
  }
}

LocalModelRawOutput
OllamaModelBackend::invoke(const LocalModelRequest &model_request) {
  model_request.validate();
  if (!config_.enabled)
    throw OllamaConfigurationError("Ollama backend is disabled");
  if (model_request.backend_id != backend_id_ ||
      model_request.model_id != binding_.ollama_model ||
      current_model_ != binding_.ollama_model) {
    throw OllamaProtocolError(
        "Ollama request does not match the loaded binding");
  }
  std::ostringstream payload;
  payload << "{\"keep_alive\":0,\"model\":"
          << local_model_json_string(model_request.model_id) << ",\"prompt\":"
          << local_model_json_string(model_request.rendered_prompt)
          << ",\"stream\":false,\"think\":false,\"options\":{\"num_predict\":"
          << config_.max_output_tokens << "}}";
  const auto response = transport_->perform(
      request(OllamaHttpMethod::post, "/api/generate", payload.str(),
              effective_receive_timeout(config_, model_request.timeout_seconds),
              config_.max_response_bytes),
      model_request.request_id);
  if (response.status_code != 200) {
    throw OllamaProtocolError("local Ollama generation returned HTTP status " +
                              std::to_string(response.status_code));
  }
  return parse_generation(response.body);
}

LocalModelRawOutput
OllamaModelBackend::parse_generation(const std::string &json) const {
  try {
    const auto root_value = runtime_detail::JsonParser(json).parse();
    const auto &root =
        runtime_detail::object(root_value, "OllamaGenerateResponse");
    const auto model = runtime_detail::string(
        runtime_detail::required(root, "model", "OllamaGenerateResponse"),
        "OllamaGenerateResponse.model");
    const auto text = runtime_detail::string(
        runtime_detail::required(root, "response", "OllamaGenerateResponse"),
        "OllamaGenerateResponse.response");
    const auto done = runtime_detail::boolean(
        runtime_detail::required(root, "done", "OllamaGenerateResponse"),
        "OllamaGenerateResponse.done");
    if (model != binding_.ollama_model || !done) {
      throw OllamaProtocolError(
          "local Ollama generation is incomplete or for another model");
    }
    return {"generation", {{"text", text}}};
  } catch (const OllamaProtocolError &) {
    throw;
  } catch (const std::exception &error) {
    throw InvalidLocalModelResponseError(
        std::string("invalid local Ollama generation response: ") +
        error.what());
  }
}

void OllamaModelBackend::cancel(const std::string &request_id) {
  transport_->cancel(request_id);
}

void OllamaModelBackend::unload(const std::string &model_id) {
  if (current_model_ == model_id)
    current_model_.clear();
}

OllamaBackendPlugin::OllamaBackendPlugin(OllamaBackendConfig config,
                                         OllamaModelBinding binding,
                                         std::function<bool()> health_probe)
    : config_(std::move(config)), binding_(std::move(binding)),
      health_probe_(std::move(health_probe)) {
  descriptor_.capability_id = "inference.backend.ollama";
  descriptor_.implementation_id = "native.ollama_loopback_backend";
  descriptor_.implementation_version = "1.0.0";
  descriptor_.kind = "optional_inference_backend";
  descriptor_.provides.push_back(
      {"infer.local_model.backend", "urn:eu-digital:local-model-response:1"});
  descriptor_.supports_hot_plug = true;
  descriptor_.supports_checkpoint = false;
}

const CapabilityDescriptor &OllamaBackendPlugin::descriptor() const {
  return descriptor_;
}

void OllamaBackendPlugin::validate_manifest() {
  config_.validate();
  binding_.validate();
  if (!config_.enabled) {
    throw OllamaConfigurationError("Ollama backend is disabled");
  }
}

void OllamaBackendPlugin::configure() {}
void OllamaBackendPlugin::initialize() {}
void OllamaBackendPlugin::calibrate() {}
bool OllamaBackendPlugin::health_check() {
  return !health_probe_ || health_probe_();
}
void OllamaBackendPlugin::start() {}
void OllamaBackendPlugin::drain() {}
std::map<std::string, std::string> OllamaBackendPlugin::checkpoint() {
  return {};
}
void OllamaBackendPlugin::stop() {}
void OllamaBackendPlugin::uninstall() {}

} // namespace eu_digital
