---
id: SPEC-051
title: "Ollama Backend Integration"
status: done
phase: cognitive_loop
dependencies: ["SPEC-013"]
adrs: []
contracts: []
---

# SPEC-051 - Ollama Backend Integration

## Objetivo
Introduzir um backend HTTP nativo (`OllamaModelBackend`) para conectar o Gateway de Modelos Locais (`LocalModelGateway`) à API REST do Ollama rodando localmente (tipicamente na porta 11434).

## Visão Geral
A arquitetura base da SPEC-013 (`LocalModelGateway`) descrevia abstrações independentes do provedor. No entanto, o ecossistema C++ ainda não continha um motor (`LocalModelBackend`) de fato. Para viabilizar a prototipação rápida, a integração HTTP via Ollama se torna o caminho mais aderente e limpo no Windows.

## Escopo negativo
- Não inclui execução direta de modelos GGUF via CUDA ou Metal.
- Não requer dependências pesadas de curl.

## Requisitos Funcionais
- Implementar `OllamaModelBackend` herdando de `LocalModelBackend`.
- O método `load()` deve verificar se a tag do modelo existe localmente usando `GET /api/tags`.
- O método `invoke()` deve construir uma requisição JSON e fazer um `POST /api/generate`.

## Critérios de aceite
- [x] O `OllamaModelBackend` é capaz de enviar requisições e processar a resposta json do servidor local Ollama.
- [ ] A arquitetura isola o protocolo HTTP dentro do `OllamaModelBackend` apenas.
- [ ] Teste mock valida a serialização correta de request/response JSON e timeouts.


