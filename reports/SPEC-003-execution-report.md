# Relatório de Execução

SPEC: SPEC-003  
Agente: Codex  
Data: 2026-07-28  
Commit: trabalho local não commitado

## Alterações realizadas

- implementada porta `SystemActivityAdapter` e normalização de snapshots;
- implementado `WindowsSystemActivityAdapter` com enumeração Win32 de
  processos e leitura da janela em foco;
- implementados eventos canônicos de mudança de foco, início e fim de
  processo;
- implementados baseline inicial, health check, orçamento médio de CPU,
  reconexão automática e estado explícito de falha/permissão;
- publicado `CapabilityDescriptor` e wrapper `SystemActivityPlugin` para o
  lifecycle de capacidades;
- adicionados testes C++ com adapter fake determinístico.

## Arquivos modificados

- `cpp/core/system_activity_sensor.hpp`;
- `cpp/tests/system_activity_sensor_test.cpp`;
- `CMakeLists.txt`;
- `cpp/README.md`;
- `docs/06-operations/DEVELOPMENT_COMMANDS.md`;
- `specs/SPEC-003-system-activity-sensor.md`.

## Testes executados

```text
cmake -S . -B build/spec003-isolated -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build/spec003-isolated
ctest --test-dir build/spec003-isolated -R system_activity_sensor --output-on-failure
PYTHONPATH=python python3 -m unittest discover -s python/tests -q
PYTHONPATH=python python3 tools/validate_contracts.py
PYTHONPATH=python python3 tools/validate_sandbox.py datasets/synthetic/v1
```

## Resultados

- teste C++ específico aprovado;
- baseline não gerou observações negativas nem eventos falsos;
- foco, processo iniciado e processo encerrado foram normalizados;
- reconexão após falha foi exercitada;
- falha de permissão retornou estado de health sem lançar exceção;
- suíte Python completa: 60 testes aprovados;
- contratos e corpus sintético validados.

## Critérios de aceite

- [x] detecta mudança de janela ativa;
- [x] detecta início e fim de processo observável;
- [x] mede uso médio contra orçamento configurável;
- [x] falha de permissão não encerra o agente.

## Desvios

O branch Win32 não foi compilado neste ambiente Linux por ausência do SDK e
toolchain Windows. A lógica é compilada e testada com adapter fake no Linux, e
o workflow híbrido existente em `windows-latest` é o gate de compilação do
adaptador real. Nenhum conteúdo de tela ou automação foi incluído.

## Riscos e pendências

- o orçamento medido em hardware Windows real ainda precisa de benchmark no
  ambiente-alvo;
- detalhes de permissões podem variar por versão do Windows;
- eventos sem observação permanecem ausência explícita, não evidência negativa.

## Decisões tomadas

- usar polling encapsulado na primeira implementação;
- manter a captura Win32 fora do núcleo cognitivo;
- publicar descriptor e lifecycle por capability plugin;
- usar adapter fake para testar normalização sem depender de estado externo.

## Evidências

- implementação: `cpp/core/system_activity_sensor.hpp`;
- teste: `cpp/tests/system_activity_sensor_test.cpp`;
- CMake/CTest: `CMakeLists.txt`;
- contratos consumidos: `contracts/schemas/canonical_event.schema.json` e
  `docs/03-contracts/CAPABILITY_DESCRIPTOR.md`.
