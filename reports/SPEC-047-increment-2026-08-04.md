# Relatório de Execução

> Atualização de 2026-08-04: a aprovação e implementação dos DTOs 1.0 resolveu
> a pendência registrada neste relatório. A evidência final está em
> `reports/SPEC-047-dto-increment-2026-08-04.md`; a SPEC-047 está concluída.

SPEC: SPEC-047 — Component Wiring, Ports and Contracts
Agente: Codex
Data: 2026-08-04
Commit: não aplicável

## Alterações realizadas

- Adicionada `IPatternLearningPort` para expor a aprendizagem incremental da
  SPEC-035 sem acoplar consumidores a `PatternLearner`.
- Adicionados DTOs internos para observação, feedback, resultado e snapshot de
  padrões, preservando todos os campos do contrato `ObservedPattern` 1.0.
- Adicionado `PatternLearnerAdapter` com serialização de acesso e erro de
  delegação estruturado.
- Estendida `CognitivePortFactory` para construir a nova porta.
- Estendido `CapabilityRegistry::register_instance` com descritor explícito,
  prioridade, rejeição de instância nula e transições que atualizam o
  self-model.
- Registrada a porta na composition root desktop sem conectá-la ainda ao
  coordenador da SPEC-045.
- Registrado `ports_mock_test` no CTest; antes ele era apenas compilado.
- Adotado `PortResult<T>` 1.0 com `PortError` após aprovação humana, de forma
  aditiva e compatível com as assinaturas legadas.
- Adicionadas operações seguras `*_result()` às portas cognitivas.
- Removido o fallback silencioso do `WorldModelAdapter`; falhas agora são
  estruturadas e não se confundem com uma predição vazia.
- Adicionado microbenchmark Release que compara chamada concreta e virtual no
  mesmo `WorldModelAdapter`.

## Arquivos modificados

- `CMakeLists.txt`
- `cpp/benchmarks/port_dispatch_benchmark.cpp`
- `cpp/core/capability_runtime.hpp`
- `cpp/core/contracts/pattern_learning.hpp`
- `cpp/core/contracts/port_result.hpp`
- `contracts/schemas/port_result.schema.json`
- `contracts/README.md`
- `docs/03-contracts/PORT_RESULT.md`
- `cpp/core/ports/iepisode_boundary_port.hpp`
- `cpp/core/ports/imemory_write_port.hpp`
- `cpp/core/ports/imemory_retrieval_port.hpp`
- `cpp/core/ports/iprediction_port.hpp`
- `cpp/core/ports/iworkspace_selection_port.hpp`
- `cpp/core/ports/imetacognition_port.hpp`
- `cpp/core/ports/iself_model_query_port.hpp`
- `cpp/core/ports/icognitive_decision_port.hpp`
- `cpp/core/ports/ipattern_learning_port.hpp`
- `cpp/core/adapters/pattern_learner_adapter.hpp`
- `cpp/core/adapters/cognitive_port_factory.hpp`
- `cpp/core/adapters/world_model_adapter.hpp`
- `cpp/shell/desktop_controller.hpp`
- `cpp/shell/desktop_controller.cpp`
- `cpp/tests/ports_mock_test.cpp`
- `cpp/tests/pattern_learner_adapter_test.cpp`
- `cpp/tests/cognitive_port_factory_test.cpp`
- `cpp/tests/port_result_test.cpp`
- testes dos adaptadores de episódio, memória, predição, workspace,
  metacognição, self-model e decisão
- `specs/SPEC-047-component-wiring-and-contracts.md`
- `plans/PLAN-047.md`
- `docs/05-governance/OPEN_QUESTIONS.md`
- `REPOSITORY_TREE.txt`

## Testes executados

- Testes Python: `uv run --frozen pytest -q` — 236 passaram.
- Tipos Python: `uv run --frozen mypy python/eu_digital_lab` — sem problemas
  nos 28 arquivos analisados.
- Lint global: `uv run --frozen ruff check .` — falhou com 57 débitos
  preexistentes, todos fora dos arquivos alterados por este incremento.
- Build focado dos alvos de portas/adapters — passou.
- Build completo do núcleo: `cmake --build build/dev --target all` — passou.
- CTest completo: `ctest --test-dir build/dev --output-on-failure` — 34 de 34
  passaram.
- Build Qt 6.7.2: `cmake --build --preset windows-qt --target all` — passou,
  incluindo `eu_digital_desktop` e a composition root alterada.
- CTest Qt offscreen: `ctest --preset windows-qt` — 36 de 36 passaram.
- Validação documental — passou com 54 SPECs e uma configuração válidas.
- Validação de SPECs — passou com 54 SPECs válidas.
- Validação de contratos — passou com o fixture `CanonicalEvent` válido.
- Parse do schema `port_result.schema.json` — passou.
- `git diff --check` — passou.
- Microbenchmark Release estabilizado — passou em sete de sete execuções. O
  protocolo usa afinidade de CPU, ciclos da thread e blocos pareados ABBA/BAAB;
  as medianas foram -0,20%, -0,27%, 0,13%, -0,86%, -0,31%, -0,57% e 0,38%,
  com mediana global aproximada de -0,27%.

## Resultados

A aprendizagem de padrões promovida possui agora uma fronteira substituível e
resolvida por operação. Ausência, prioridade, substituição, remoção e
reinstalação foram verificadas, inclusive no self-model do registry.

## Critérios de aceite

- Os sete critérios funcionais/contratuais atualizados na SPEC-047 passam.
- O critério de desempenho passa sem relaxar o limite de 1%.
- A classe promovida `PatternLearner` não foi alterada.
- O contrato público `ObservedPattern` permanece na versão 1.0.
- A recuperação uniforme das portas antigas usa agora contrato versionado e
  compatível.
- A SPEC permanece `in_progress` porque quatro adapters ainda fabricam dados
  que não existem em suas entradas, impedindo demonstrar delegação fiel.

## Desvios

- O kit Qt 6.7.2 inclui um `libc++.dll` mais antigo que o compilador LLVM 22.
  Os testes exigem o diretório `bin` do compilador antes do diretório `bin` do
  Qt no `PATH`; com essa ordem, todos os testes passaram.
- O lint global não está verde devido a 57 ocorrências preexistentes em Python;
  corrigi-las está fora do escopo da SPEC-047. Não houve alteração Python neste
  incremento.

## Riscos e pendências

- Resolver a questão aberta 25 antes de alterar as entradas públicas das portas
  ou remover valores fabricados pelos adapters.

## Decisões tomadas

- Usar a operação já publicada pelo plugin promovido: `learn.patterns`.
- Preservar o `PatternLearner` concreto sem herança nem alteração semântica.
- Não conectar a nova porta ao `CognitiveCoordinator`; essa mudança pertence à
  SPEC-045.
- Adotar `PortResult<T>` 1.0 como extensão compatível, conforme aprovação humana
  de 2026-08-04.
- Manter o benchmark fora do CTest, usar ciclos efetivos da thread e preservar
  retorno não zero quando a mediana pareada excede 1%.
- Não tratar sucesso do benchmark como substituto para mapeamento contratual
  fiel dos adapters.

## Evidências

- `cpp/tests/pattern_learner_adapter_test.cpp`
- `cpp/tests/cognitive_port_factory_test.cpp`
- `cpp/tests/port_result_test.cpp`
- saída local do CTest: 34/34 testes aprovados.
- saída local das sete execuções do benchmark Release estabilizado.
