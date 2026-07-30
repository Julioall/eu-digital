---
id: SPEC-015
title: Áudio ambiente
status: done
phase: 9
dependencies: [SPEC-002, SPEC-023]
adrs: [ADR-0017]
contracts: [AUDIO_SENSOR_SCHEMA.md]
---

# SPEC-015 — Áudio ambiente

Status: done
Fase: 9
Dependências: SPEC-002, SPEC-023
ADR aplicável: `ADR-0017-local-audio-sensor-ports.md`
Contrato afetado: `AUDIO_SENSOR_SCHEMA.md`

## Objetivo
Capturar áudio local, detectar fala e produzir transcrições temporais.

## Requisitos
- VAD local.
- Transcrição local.
- Segmentos com confiança.
- Associação temporal com demais eventos.
- Áudio bruto referenciado.

## Escopo negativo
Inferência de verdade, intenção ou emoção apenas pela voz.

## Critérios de aceite
- [x] Segmentos possuem timestamps.
- [x] Transcrição falha não quebra timeline.
- [x] Custo de processamento é medido.

## Implementação e testes

- cpp/core/audio_sensor.hpp implementa as portas locais e o plugin removível.
- cpp/tests/audio_sensor_test.cpp cobre segmento temporal, referência/hash,
  falha de transcrição, ausência de sinal, falha de VAD/captura e custo.
- contracts/schemas/audio_segment.schema.json e
  contracts/schemas/audio_transcription.schema.json versionam os payloads.
