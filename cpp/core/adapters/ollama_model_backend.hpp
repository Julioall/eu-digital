#pragma once

#include "core/local_model_gateway.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace eu_digital {

inline constexpr const char *OLLAMA_BACKEND_SCHEMA_VERSION = "1.0";
inline constexpr const char *OLLAMA_BACKEND_COMPATIBILITY =
    "ollama.http.loopback.v1";

class OllamaConfigurationError : public LocalModelGatewayError {
public:
  explicit OllamaConfigurationError(const std::string &message)
      : LocalModelGatewayError(message) {}
};

class OllamaProtocolError : public LocalModelGatewayError {
public:
  explicit OllamaProtocolError(const std::string &message)
      : LocalModelGatewayError(message) {}
};

struct OllamaBackendConfig {
  std::string schema_version{OLLAMA_BACKEND_SCHEMA_VERSION};
  bool enabled{false};
  std::string host{"127.0.0.1"};
  std::uint16_t port{11434};
  std::uint32_t connect_timeout_ms{2000};
  std::uint32_t send_timeout_ms{5000};
  std::uint32_t receive_timeout_ms{120000};
  std::uint32_t max_output_tokens{512};
  std::size_t max_response_bytes{1024 * 1024};
  std::size_t max_tags_bytes{4 * 1024 * 1024};

  void validate() const;
  static OllamaBackendConfig from_json(const std::string &json);
};

struct OllamaModelBinding {
  std::string schema_version{OLLAMA_BACKEND_SCHEMA_VERSION};
  std::string ollama_model;
  std::string ollama_digest;
  std::uint64_t ollama_size_bytes{0};
  LocalModelArtifact artifact;

  void validate() const;
  static OllamaModelBinding from_json(const std::string &json);
};

enum class OllamaHttpMethod { get, post };

struct OllamaHttpRequest {
  OllamaHttpMethod method{OllamaHttpMethod::get};
  std::string path;
  std::string body;
  std::uint32_t connect_timeout_ms{0};
  std::uint32_t send_timeout_ms{0};
  std::uint32_t receive_timeout_ms{0};
  std::size_t max_response_bytes{0};

  void validate() const;
};

struct OllamaHttpResponse {
  std::uint32_t status_code{0};
  std::string body;
};

class IOllamaTransport {
public:
  virtual ~IOllamaTransport() = default;
  virtual OllamaHttpResponse perform(const OllamaHttpRequest &request,
                                     const std::string &request_id) = 0;
  virtual void cancel(const std::string &request_id) = 0;
};

class WinHttpOllamaTransport final : public IOllamaTransport {
public:
  explicit WinHttpOllamaTransport(const OllamaBackendConfig &config);
  ~WinHttpOllamaTransport() override;

  WinHttpOllamaTransport(const WinHttpOllamaTransport &) = delete;
  WinHttpOllamaTransport &operator=(const WinHttpOllamaTransport &) = delete;

  OllamaHttpResponse perform(const OllamaHttpRequest &request,
                             const std::string &request_id) override;
  void cancel(const std::string &request_id) override;

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

class OllamaModelBackend final : public LocalModelBackend {
public:
  OllamaModelBackend(OllamaBackendConfig config, OllamaModelBinding binding);
  OllamaModelBackend(std::shared_ptr<IOllamaTransport> transport,
                     OllamaBackendConfig config, OllamaModelBinding binding);

  const std::string &backend_id() const override;
  void load(const LocalModelArtifact &artifact) override;
  LocalModelRawOutput invoke(const LocalModelRequest &request) override;
  void cancel(const std::string &request_id) override;
  void unload(const std::string &model_id) override;

  const OllamaBackendConfig &config() const { return config_; }
  const OllamaModelBinding &binding() const { return binding_; }

private:
  OllamaHttpRequest request(OllamaHttpMethod method, std::string path,
                            std::string body, std::uint32_t receive_timeout_ms,
                            std::size_t max_response_bytes) const;
  void verify_catalog(const std::string &json) const;
  LocalModelRawOutput parse_generation(const std::string &json) const;

  std::shared_ptr<IOllamaTransport> transport_;
  OllamaBackendConfig config_;
  OllamaModelBinding binding_;
  std::string backend_id_{"ollama"};
  std::string current_model_;
};

class OllamaBackendPlugin final : public CapabilityPlugin {
public:
  explicit OllamaBackendPlugin(OllamaBackendConfig config,
                               OllamaModelBinding binding,
                               std::function<bool()> health_probe = {});

  const CapabilityDescriptor &descriptor() const override;
  void validate_manifest() override;
  void configure() override;
  void initialize() override;
  void calibrate() override;
  bool health_check() override;
  void start() override;
  void drain() override;
  std::map<std::string, std::string> checkpoint() override;
  void stop() override;
  void uninstall() override;

private:
  CapabilityDescriptor descriptor_;
  OllamaBackendConfig config_;
  OllamaModelBinding binding_;
  std::function<bool()> health_probe_;
};

} // namespace eu_digital
