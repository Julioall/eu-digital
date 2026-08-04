---
id: SPEC-051
title: "Ollama Backend Integration"
status: done
phase: cognitive_loop
dependencies: ["SPEC-013"]
adrs: ["ADR-0015", "ADR-0036"]
contracts: [LOCAL_MODEL_GATEWAY_SCHEMA.md, OLLAMA_BACKEND_CONTRACT.md, ollama_backend_config.schema.json, ollama_model_binding.schema.json]
---

# SPEC-051 - Ollama Backend Integration

## Objetivo
Introduzir um backend HTTP nativo (`OllamaModelBackend`) para conectar o Gateway de Modelos Locais (`LocalModelGateway`) à API REST do Ollama rodando localmente (tipicamente na porta 11434).

## Visão Geral
A arquitetura base da SPEC-013 (`LocalModelGateway`) descrevia abstrações independentes do provedor. No entanto, o ecossistema C++ ainda não continha um motor (`LocalModelBackend`) de fato. Para viabilizar a prototipação rápida, a integração HTTP via Ollama se torna o caminho mais aderente e limpo no Windows.

## Escopo negativo

- Não baixa, cria, copia, publica ou remove modelos.
- Não aceita host remoto, proxy, redirect, modelo cloud ou API key.
- Não altera o núcleo cognitivo nem apresenta texto diretamente na UI.
- Não inclui execução direta de modelos GGUF via CUDA ou Metal.
- Não requer dependências pesadas de curl.

## Requisitos Funcionais

- Configuração e binding do modelo devem ser DTOs 1.0 validados antes de rede.
- O transporte HTTP deve ser injetável e substituível em testes.
- A integração deve publicar um `CapabilityDescriptor` e degradar com segurança.
- Implementar `OllamaModelBackend` herdando de `LocalModelBackend`.
- O método `load()` deve verificar se a tag do modelo existe localmente usando `GET /api/tags`.
- O método `invoke()` deve construir uma requisição JSON e fazer um `POST /api/generate`.

## Critérios de aceite
- [x] O `OllamaModelBackend` é capaz de enviar requisições e processar a resposta JSON do servidor local Ollama.
- [x] A arquitetura isola o protocolo HTTP dentro do `OllamaModelBackend` apenas.
- [x] Teste mock valida a serialização correta de request/response JSON e timeouts.
- [x] `load()` rejeita modelo ausente ou divergente sem executar pull implícito.
- [x] Host, porta, proxy, redirects, endpoints e tamanho de corpo respeitam a ADR-0036.
- [x] JSON válido, truncado, malformado, duplicado e com tipos incorretos é coberto.
- [x] Cancelamento fecha a requisição ativa e o gateway preserva um worker/modelo pesado.
- [x] Ausência, falha, remoção, reinstalação e substituição da capacidade são testadas.
- [x] Configuração desabilitada ou modelo ausente preserva o fallback seguro do produto.


