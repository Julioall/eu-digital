---
id: SPEC-040
title: Modelo local CPU-first e diálogo estruturado
status: future
phase: beta
dependencies: [SPEC-013, SPEC-014, SPEC-039]
adrs: [ADR-0003, ADR-0004, ADR-0009, ADR-0010, ADR-0011]
contracts: [LOCAL_MODEL_GATEWAY_SCHEMA.md, local_model_request.schema.json, local_model_response.schema.json, model_prompt_template.schema.json]
---

# SPEC-040 — Modelo local CPU-first e diálogo estruturado

## Objetivo

Integrar um backend C++ local CPU-first, quantizado e opcional, com fila de um
modelo pesado, timeout, cancelamento, descarga, schema e degradação explícita.

## Escopo negativo

Não usar API externa, rede, fallback por regras para simular semântica, modelo
obrigatório para iniciar o runtime ou LLM como núcleo cognitivo.

## Escopo

Inclui seleção GGUF por benchmark congelado, licença, português, saída
estruturada, validação de hash/compatibilidade/licença, artefato de modelo
separado e diálogo textual mínimo.

## Protocolo científico/operacional

Hipótese operacional: backend local dentro dos limites mantém latência e memória
aceitáveis. Baseline: ausência explícita de modelo. Métricas: schema válido,
latência p50/p95, RAM pico, cancelamento e fila. Ablação: modelo ausente e
descarga entre requisições. Falsificação: incompatibilidade, vazamento, timeout
ou violação de limite.

## Critérios de aceite

- [ ] Modelo até 4 GiB, RAM pico até 7 GiB e licença compatível.
- [ ] Hash, compatibilidade, schema, timeout, cancelamento e descarga passam.
- [ ] Sem modelo, timeline/privacidade/diagnóstico continuam disponíveis e
      diálogo/sugestões dependentes ficam desativados.
- [ ] Runtime e payload de modelo são artefatos assinados separados.
- [ ] Nenhuma API externa ou fallback semântico é introduzido.

## Saída

Backend local opcional e diálogo textual estruturado, sem avatar ou sugestões
orquestradas ainda.
