#include "core/adapters/ollama_model_backend.hpp"

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace {

std::string read_text(const std::string &path) {
  std::ifstream input(path, std::ios::binary);
  if (!input)
    throw std::runtime_error("cannot read probe input: " + path);
  std::ostringstream text;
  text << input.rdbuf();
  return text.str();
}

} // namespace

int main(int argc, char **argv) {
  using namespace eu_digital;
  if (argc != 3) {
    std::cerr << "usage: ollama_live_probe <config.json> <binding.json>\n";
    return 2;
  }
  try {
    const auto config = OllamaBackendConfig::from_json(read_text(argv[1]));
    const auto binding = OllamaModelBinding::from_json(read_text(argv[2]));
    OllamaModelBackend backend(config, binding);
    LocalModelGateway gateway({{"ollama", &backend}}, {"ollama"},
                              binding.artifact);
    const ModelPromptTemplate prompt_template{
        "ollama-live-probe", "1.0.0", "{content}", {"content"}};
    LocalModelRequest request{
        "ollama-live-probe",
        "ollama",
        binding.ollama_model,
        0,
        120.0,
        prompt_template,
        "Responda somente com a palavra OK, sem pontuacao."};
    const auto response = gateway.invoke(request);
    gateway.close();
    const auto found = response.output_fields.find("text");
    if (response.output_kind != "generation" ||
        found == response.output_fields.end() || found->second.empty()) {
      throw InvalidLocalModelResponseError("probe returned no local text");
    }
    std::cout << "ollama_live_probe: ok; response_bytes="
              << found->second.size() << '\n';
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "ollama_live_probe: failed: " << error.what() << '\n';
    return 1;
  }
}
