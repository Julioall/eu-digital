# Relatório de Execução

SPEC: SPEC-004  
Agente: Codex  
Data: 2026-07-28  
Commit: trabalho local não commitado

## Alterações realizadas

- implementados tipos de eventos brutos de teclado, mouse e clipboard;
- implementado agregador configurável com contadores, distância de mouse,
  atalhos, taxa de digitação e pausas;
- adicionada associação de todos os payloads ao contexto da janela ativa;
- adicionada serialização de payload versionada (`1.0`);
- clipboard é emitido em evento separado com tamanho e digest, sem conteúdo;
- adicionados callbacks e hooks Win32 opcionais para captura de teclado/mouse;
- adicionado `CapabilityDescriptor` e `InputInteractionPlugin`;
- adicionados testes C++ determinísticos.

## Arquivos modificados

- `cpp/core/input_interaction_sensor.hpp`;
- `cpp/tests/input_interaction_sensor_test.cpp`;
- `CMakeLists.txt`;
- `cpp/README.md`;
- `docs/06-operations/DEVELOPMENT_COMMANDS.md`;
- `specs/SPEC-004-input-interaction-sensor.md`.

## Testes executados

```text
cmake -S . -B build/spec004-isolated -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build/spec004-isolated
ctest --test-dir build/spec004-isolated -R input_interaction --output-on-failure
PYTHONPATH=python python3 -m unittest discover -s python/tests -q
PYTHONPATH=python python3 tools/validate_contracts.py
PYTHONPATH=python python3 tools/validate_sandbox.py datasets/synthetic/v1
```

## Resultados

- teste C++ específico aprovado;
- eventos brutos e agregados exercitados;
- métricas de alto volume preservadas;
- taxa, pausa e atalho registrados;
- contexto de janela presente nos eventos;
- clipboard separado e sem texto armazenado;
- suíte Python completa: 60 testes aprovados;
- contratos e corpus sintético validados.

## Critérios de aceite

- [x] eventos possuem contexto de janela;
- [x] alto volume é agregado sem perda de métricas;
- [x] clipboard produz evento separado.

## Desvios

O ambiente Linux valida a normalização com callbacks determinísticos. A
instalação dos hooks Win32 é compilada apenas no branch `_WIN32` e depende do
workflow Windows existente; nenhum conteúdo de clipboard é persistido.

## Riscos e pendências

- o orçamento real de hooks e a taxa máxima de eventos precisam de benchmark
  em Windows;
- o contexto de janela depende da captura ativa fornecida pelo adapter;
- interpretação de intenção e execução de entrada permanecem fora do escopo.

## Decisões tomadas

- manter captura bruta opcional e agregação sempre disponível;
- preservar contadores mesmo quando eventos brutos são emitidos;
- registrar apenas digest/tamanho de clipboard;
- não adicionar APIs de automação ou execução de input.

## Evidências

- implementação: `cpp/core/input_interaction_sensor.hpp`;
- teste: `cpp/tests/input_interaction_sensor_test.cpp`;
- CMake/CTest: `CMakeLists.txt`;
- contrato consumido: `docs/03-contracts/EVENT_SCHEMA.md`.
