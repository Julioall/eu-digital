# SPEC-051: Ollama Backend Integration

## Visão Geral
Esta especificação introduz um backend HTTP nativo (`OllamaModelBackend`) para conectar o Gateway de Modelos Locais (`LocalModelGateway`) à API REST do Ollama rodando localmente (tipicamente na porta 11434).

## Motivação
A arquitetura base da SPEC-013 (`LocalModelGateway`) descrevia abstrações independentes do provedor. No entanto, o ecossistema C++ ainda não continha um motor (`LocalModelBackend`) de fato. Para viabilizar a prototipação rápida e usar modelos Vision-Language (como o `qwen3-vl:2b`) de forma imediata sem inchar o projeto com bibliotecas GGUF pesadas (como `llama.cpp` e dependências CUDA/Metal), a integração HTTP via Ollama se torna o caminho mais aderente e limpo no Windows.

## Requisitos Funcionais
- Implementar `OllamaModelBackend` herdando de `LocalModelBackend`.
- O método `load()` deve verificar se a tag do modelo (ex: `qwen3-vl:2b`) existe localmente usando `GET /api/tags`.
- O método `invoke()` deve construir uma requisição JSON (com suporte a *images*, caso disponíveis, extraídas dos módulos de visão no futuro) e fazer um `POST /api/generate`.
- Timeout e cancelamento devem ser tratados (embora requisições bloqueantes do WinHTTP/WinINet possuam limitações, deve-se usar sockets primitivos ou configurar timeouts no cliente HTTP usado).

## Requisitos Não Funcionais
- **Nenhuma dependência massiva de HTTP cURL:** Dado que estamos focando num build rápido e livre de poluição, usaremos APIs nativas de rede (como `WinHTTP` no Windows) ou um client HTTP socket C++ puro (`httplib.h` header-only ou simulação via Boost/ASIO, se já constarem no sistema). Se nada disso estiver disponível de forma simples, usaremos implementações baseadas no cabeçalho `<windows.h>` WinHTTP já disponível nativamente no ecossistema MSVC.

## Contratos
- A classe injetará os outputs do Ollama num `LocalModelRawOutput` respeitando a SPEC-013.

## Critérios de Aceite
- [ ] A arquitetura isola o protocolo HTTP dentro do `OllamaModelBackend` apenas.
- [ ] Teste mock valida a serialização correta de request/response JSON e timeouts.
