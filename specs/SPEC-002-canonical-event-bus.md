---
id: SPEC-002
title: Barramento e evento canônico
status: blocked
phase: 0
dependencies: [SPEC-001]
adrs: []
contracts: [EVENT_SCHEMA.md]
---

# SPEC-002 — Barramento e evento canônico

Status: blocked
Fase: 0
Dependências: SPEC-001
Contratos: `EVENT_SCHEMA.md`

## Objetivo
Implementar publicação, consumo e persistência básica de `CanonicalEvent`.

## Requisitos
- Event bus assíncrono em processo.
- Assinaturas por tipo e fonte.
- Idempotência por `event_id`.
- Backpressure explícito.
- Dead-letter queue local.
- Replay em testes.

## Escopo negativo
Mensageria distribuída e rede.

## Critérios de aceite
- [ ] Ordem preservada por fonte.
- [ ] Evento duplicado não é processado duas vezes.
- [ ] Consumidor lento não derruba produtor.
- [ ] Evento inválido é rejeitado com erro tipado.
