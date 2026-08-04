# Relatório de Execução

SPEC: SPEC-047 — Component Wiring, Ports and Contracts
Agente: Codex
Data: 2026-08-04
Commit: commit que contém este relatório

## Alterações realizadas

- Criados DTOs 1.0 para observação e escrita de episódio, recuperação de
  memória, seleção de workspace, avaliação metacognitiva e decisão.
- Adicionadas operações aditivas às seis portas afetadas, com retorno por
  `PortResult<T>` e preservação das assinaturas legadas.
- Substituídos dados artificiais nos adapters por mapeamento dos campos
  fornecidos e delegação aos componentes promovidos.
- Chamadas legadas insuficientes agora falham de forma estruturada.
- Mantida fora do escopo a migração do `CognitiveCoordinator` da SPEC-045.

## Arquivos modificados

- `cpp/core/contracts/cognitive_port_requests.hpp`;
- seis interfaces em `cpp/core/ports/` e seus seis adapters em
  `cpp/core/adapters/`;
- testes dos adapters e `cpp/tests/cognitive_port_requests_test.cpp`;
- sete schemas em `contracts/schemas/`;
- `docs/03-contracts/COGNITIVE_PORT_REQUESTS.md`;
- SPEC, plano, questões abertas, índice de contratos e árvore do repositório.

## Testes executados

- Build completo do núcleo: passou.
- CTest do núcleo: 35/35 passaram.
- Build do preset `windows-qt`, incluindo desktop: passou.
- CTest Qt offscreen com `bin` do LLVM antes do `bin` do Qt: 37/37 passaram.
- Python: 236 testes passaram.
- Mypy: 28 arquivos sem problemas.
- Documentação: 13/13 testes passaram.
- Validador de SPECs: 54 SPECs válidas.
- Validador de contratos: fixture `CanonicalEvent` válida.
- Parse JSON dos schemas de porta: passou.
- Árvore do repositório: 665 arquivos, validada.
- `git diff --check`: passou.
- Ruff global: 57 débitos preexistentes fora dos arquivos alterados; nenhum
  arquivo Python foi modificado neste incremento.
- Microbenchmark Release da evidência anterior desta SPEC: sete de sete
  execuções abaixo do limite de 1%, mediana global aproximada de -0,27%.

## Resultados

Episódio, memória, workspace, metacognição e decisão possuem entradas
suficientes para delegação real. Nenhum dos adapters afetados cria episódio,
hipótese, tensão, timestamp ou evidência ausente. Resultados enriquecidos usam
tipos 1.0 novos; contratos legados mantêm o layout anterior.

## Critérios de aceite

- Todos os critérios da SPEC-047 estão marcados e verificados.
- Ausência, substituição, remoção, reinstalação e falha estruturada permanecem
  cobertas pelos testes anteriores da SPEC.
- O critério de desempenho permanece atendido.
- A questão aberta 25 foi resolvida pela aprovação humana dos DTOs 1.0.

## Desvios

- A primeira execução Qt sem o diretório de DLLs do Qt no `PATH` falhou ao
  carregar dois executáveis. A execução normativa, com LLVM antes de Qt para
  evitar a `libc++.dll` incompatível, passou 37/37.
- `jsonschema` não faz parte das dependências congeladas; os schemas foram
  verificados por parse JSON, pelos validadores do repositório e pelos testes
  de invariantes C++ correspondentes.

## Riscos e pendências

- A SPEC-045 ainda deve migrar o coordenador para as operações 1.0 antes de
  remover as assinaturas legadas.
- Os 57 avisos Ruff globais continuam como dívida anterior e não foram
  corrigidos para preservar o princípio de menor mudança.

## Decisões tomadas

- Manter compatibilidade compilável sem aceitar semanticamente entradas
  incompletas nos adapters concretos.
- Usar respostas 1.0 novas para memória e metacognição, evitando alterar DTOs
  públicos legados sem versionamento.
- Tratar `workspace_snapshot_id` como referência de evidência única em
  metacognição e decisão.
- Não conectar as novas operações ao coordenador nesta SPEC.

## Evidências

- `cpp/tests/cognitive_port_requests_test.cpp`;
- testes focados dos seis adapters;
- `docs/03-contracts/COGNITIVE_PORT_REQUESTS.md`;
- saídas locais de CTest 35/35 e Qt 37/37;
- `reports/SPEC-047-increment-2026-08-04.md` para as evidências anteriores de
  registry, `PortResult<T>` e microbenchmark.
