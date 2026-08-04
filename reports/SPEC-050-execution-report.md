# Relatório de Execução

SPEC: SPEC-050 — Desktop Application and Vertical Slice
Agente: Codex (GPT-5)
Data: 2026-08-04
Commit de base: `df323a0`
Estado: `in_progress`

## Alterações realizadas

- Substituído o booleano de consentimento em `QSettings` por
  `ConsentLedger` granular e persistido com DPAPI.
- Adicionados lifecycle e DTOs 1.0 para estado da sessão e amostras de
  performance, sem conteúdo sensorial.
- Sensores concretos passam a ser construídos e a publicar somente após o
  grant do par sensor/finalidade; pausa e revogação bloqueiam antes do event
  bus e atualizam o self-model de capacidades.
- Removidos manifesto/timestamp/timeline placeholders. O manifesto é gerado no
  build, instalado com o executável e dados mutáveis usam o diretório local do
  usuário.
- Adicionado marker atômico de sessão, detecção de shutdown incompleto,
  recovery via startup do runtime e remoção após shutdown limpo.
- Adicionado tratamento de suspend/resume do Windows, transições estruturadas,
  stress com watchdog e logs de erro JSON.
- O componente de instalação `desktop` inclui Qt, plugins e runtimes LLVM; o
  componente `runtime` continua isolado.
- O validador compartilhado passou a executar `allOf/if/then/else`, construção
  já usada por contratos existentes mas antes ignorada.
- Adicionado gate `qwindows` real com relatório JSON para teclado, input method,
  acessibilidade, tray, telas, DPI e estilos nativos de click-through/foco.
- Corrigido o HWND do avatar para aplicar `WS_EX_NOACTIVATE`; a primeira
  execução nativa detectou que a flag Qt isolada não produzia esse estilo.
- Removida a experiência antecipada de atividade/cards da SPEC-053 e seu
  consumidor de DTO legado; diálogo continua pela `IPresentationPort` da
  SPEC-048.

## Arquivos modificados

- Build/pacote: `CMakeLists.txt`, `config/runtime_manifest.desktop.json.in`,
  `cpp/tests/desktop_install_tree_test.cmake`,
  `cpp/tests/runtime_install_tree_test.cmake`.
- Runtime desktop: `cpp/app/eu_digital_desktop.cpp`,
  `cpp/shell/desktop_controller.*`, `cpp/shell/desktop_runtime_lifecycle.*`,
  `cpp/core/input_interaction_sensor.hpp`.
- Qt: `cpp/shell/qt_avatar_window.cpp`, `cpp/shell/qt_tray_adapter.hpp`,
  `cpp/shell/tray_state_machine.hpp`, `cpp/shell/tray_widget.*`.
- Testes: `cpp/tests/desktop_runtime_lifecycle_test.cpp`,
  `cpp/tests/desktop_integration_test.cpp`,
  `cpp/tests/qt_avatar_shell_test.cpp`,
  `python/tests/test_desktop_runtime_contracts.py`.
- Contratos/validação: `contracts/schemas/desktop_session_state.schema.json`,
  `contracts/schemas/desktop_performance_sample.schema.json`,
  `python/eu_digital_lab/schema_validation.py`.
- Governança/documentação: ADR-0037, `DESKTOP_RUNTIME_CONTRACT.md`, SPEC-050,
  `DEVELOPMENT_COMMANDS.md`, `QT_AVATAR_SHELL_MATRIX.md` e
  `REPOSITORY_TREE.txt`.

## Testes executados

- `cmake --build --preset windows-qt`
- `ctest --test-dir build/windows-qt --output-on-failure`
- `ctest --test-dir build/windows-qt -R "^qt_avatar_shell_windows"`
- `python -m pytest python/tests -q`
- `python -m mypy python/eu_digital_lab python/reference`
- Ruff check global do repositório
- `python tools/validate_contracts.py`
- validadores de documentação, SPECs e configuração
- `git diff --check`

## Resultados

- CTest Windows/Qt: 57/57 passaram, incluindo `qwindows` nativo em 100%,
  125%, 150%, 200% e 250%.
- Python: 251/251 passaram.
- Mypy: 29 arquivos, sem erros.
- Ruff global: passou sem ocorrências.
- Documentação: 13/13; 54 SPECs e 1 configuração válidas.
- Instalação: componentes `desktop` e `runtime` passaram em árvores isoladas.
- Probe nativo: pt-BR instalado, uma tela física 1920x1080/DPR 1.0, tray
  disponível, input Unicode, Tab, `QAccessible::EditableText`,
  `WS_EX_TRANSPARENT` e `WS_EX_NOACTIVATE` passaram.
- Advertência C++ herdada: comparação tautológica em
  `privacy_storage.hpp:249`.
- `windeployqt` informa ausência opcional de `dxcompiler.dll/dxil.dll`; o
  pacote usa o fallback `D3Dcompiler_47.dll/opengl32sw.dll` instalado pelo Qt.

## Critérios de aceite

- Passam: deny-by-default sem eventos, grants independentes/DPAPI,
  pausa/revogação, ausência de modelo, manifesto e relógios reais, marker e
  recovery, stress/watchdog, pacote nativo, métricas de performance e todos os
  gates globais de build/lint/tipos/testes.
- Pendente: movimento multi-monitor, Narrator/NVDA ou cliente UI Automation
  externo, compositor visual e Sleep/Hibernate físicos. Dead keys reais e
  escala física diferente de 100% também ainda não foram observados. Portanto
  a SPEC não foi marcada como concluída.

## Desvios

- TSan não está disponível na toolchain Windows/Qt atual. O gate substitutivo
  executado foi stress cross-thread repetido com watchdog e shutdown limitado,
  conforme ADR-0037; TSan continua adicional em toolchain compatível.
- A medição gráfica é offscreen Debug. Ela é gate de regressão, não substitui
  validação física nem representa build Release de distribuição.

## Riscos e pendências

- O ambiente automatizado expôs uma única tela; não há evidência válida de
  movimento entre monitores com DPIs distintos.
- A UI de settings ainda deriva os sensores instalados do registry; o ledger e
  a API suportam grants parciais, mas a revisão física deve confirmar o fluxo
  completo de reativação por sensor.
- O warning de quota em `privacy_storage.hpp` permanece herdado; o débito Ruff
  global foi encerrado por correções mecânicas validadas em commit isolado.

## Decisões tomadas

- ADR-0037 foi aceita sob a autoridade delegada pelo responsável humano.
- Consentimento é deny-by-default e nunca migra automaticamente o booleano
  legado.
- Ausência de modelo é capacidade opcional degradada; não há download, rede ou
  chamada direta da UI ao Ollama.
- Métricas operacionais não são apresentadas como evidência cognitiva.
- Escalas Qt injetadas não são descritas como DPI físico; a matriz separa as
  duas evidências.
- Código de atividade/cards da SPEC-053 não permanece no produto enquanto a
  SPEC estiver `draft` e sem contratos aprovados.

## Evidências

Última amostra automatizada (`Windows Qt 6 offscreen Debug`):

- tray activation p99: 4,3596 ms (limite < 50 ms; 200 amostras);
- frame p95: 1,5899 ms (limite < 16,6 ms; 200 amostras);
- frame p99: 1,8084 ms (limite < 33 ms; 200 amostras);
- idle CPU: 0,4885% (limite < 1%);
- shutdown: 20 ms (watchdog < 2.000 ms).

O JSONL reproduzível foi emitido em
`build/windows-qt/desktop_performance_samples.jsonl`; a matriz de plataforma
está em `docs/06-operations/QT_AVATAR_SHELL_MATRIX.md`. Os cinco relatórios
`qwindows` estão em
`build/windows-qt/qt_windows_platform_probe*.json` e registram DPRs 1.0, 1.25,
1.5, 2.0 e 2.5 com todos os invariantes aprovados.
