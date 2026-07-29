# Relatório de Execução

SPEC: SPEC-015 — Áudio ambiente
Agente: Codex
Data: 2026-07-29
Commit: feat: complete SPEC-015 local audio sensor

## Alterações realizadas

Foi implementado o menor incremento C++ para captura local por portas,
detecção de fala, segmentos temporais, transcrição local substituível e
medição de custo. O evento de segmento mantém a timeline quando a transcrição
falha; áudio bruto é apenas referenciado por URI e hash.

## Arquivos modificados

- cpp/core/audio_sensor.hpp
- cpp/tests/audio_sensor_test.cpp
- CMakeLists.txt
- contracts/schemas/audio_segment.schema.json
- contracts/schemas/audio_transcription.schema.json
- docs/03-contracts/AUDIO_SENSOR_SCHEMA.md
- docs/04-adrs/ADR-0017-local-audio-sensor-ports.md
- docs/02-architecture/COMPONENT_CATALOG.md
- docs/05-governance/OPEN_QUESTIONS.md
- docs/06-operations/DEVELOPMENT_COMMANDS.md
- contracts/README.md
- specs/SPEC-015-audio-sensor.md

## Testes executados

- python tools/validate_contracts.py
- python -m unittest discover -s python/tests -v
- cmake --build build/dev --target audio_sensor_test
- ctest --test-dir build/dev -R audio_sensor --output-on-failure
- python tools/validate_hybrid.py
- ruff check .
- ruff format --check .
- mypy python/eu_digital_lab python/tests

## Resultados

- Contrato canônico: passou.
- Testes Python: 128 passaram.
- CTest: 9/9 passaram, incluindo audio_sensor.
- Build e instalação híbridos: passaram; a release não contém Python.
- ruff check .: falhou com 20 violações preexistentes em arquivos Python
  fora da SPEC-015.
- ruff format --check .: falhou por arquivos Python preexistentes fora da
  SPEC-015.
- mypy: falhou com 6 erros preexistentes em testes/infra Python fora da
  SPEC-015.
- Validadores documentais PowerShell: não executados porque pwsh não está
  instalado neste ambiente.

## Critérios de aceite

- [x] Segmentos possuem timestamps.
- [x] Transcrição falha não quebra timeline.
- [x] Custo de processamento é medido.

## Desvios

Nenhum desvio funcional. O sensor não inclui backend de microfone, codec ou
modelo de fala concreto; essas integrações permanecem adaptadores locais
substituíveis, conforme ADR-0017.

## Riscos e pendências

- A política de retenção do áudio bruto continua aberta para decisão do
  adaptador local.
- A qualidade de VAD e transcrição requer calibração e avaliação posterior;
  confiança operacional não é evidência de verdade, intenção ou emoção.
- O lint e os tipos Python globais permanecem pendentes em alterações
  anteriores e não foram misturados neste commit.

## Decisões tomadas

- Runtime de produção em C++ por portas AudioCapture,
  VoiceActivityDetector e LocalSpeechToText.
- Falhas de captura/VAD são eventos estruturados; falhas de transcrição
  preservam o segmento e emitem evento separado.
- Nenhum byte de áudio é copiado para o evento canônico e nenhum serviço
  externo é utilizado.

## Evidências

- ADR: docs/04-adrs/ADR-0017-local-audio-sensor-ports.md.
- Contratos: contracts/schemas/audio_segment.schema.json e
  contracts/schemas/audio_transcription.schema.json.
- Teste nativo: cpp/tests/audio_sensor_test.cpp.
