# Relatório de Execução

SPEC: SPEC-028 — Runtime local mínimo do Cérebro Implantado
Agente: Codex
Data: 2026-07-29
Commit: pendente de criação

## Alterações realizadas

- Implementado `RuntimeHost` C++ local com estados explícitos, início/parada
  idempotentes, manifesto, health JSON, event bus e timeline SQLite.
- Adicionados fixtures válidos e inválidos para `RuntimeManifest`.
- Substituído o validador de fixture por CLI nativa compatível, com modos
  `--run` e `--replay`; erros são JSON estruturado em stderr.
- Adicionados testes de ciclo de vida, degradação por capacidade opcional,
  falha de dependência, replay determinístico, reinício, contratos e árvore
  de instalação sem artefatos Python.
- Incluído preset `windows-dev`, isolado do cache Linux/WSL, e documentação
  de operação, diagnóstico, pacote e remoção reversível.

## Arquivos modificados

- `cpp/core/runtime_host.hpp`
- `cpp/app/main.cpp`
- `cpp/tests/runtime_host_test.cpp`
- `cpp/tests/runtime_install_tree_test.cmake`
- `CMakeLists.txt` e `CMakePresets.json`
- `contracts/fixtures/runtime_manifest.json` e
  `contracts/fixtures/runtime_manifest.invalid.json`
- `cpp/README.md`, `docs/06-operations/DEVELOPMENT_COMMANDS.md`,
  `tools/Invoke-WindowsRuntimeValidation.ps1`,
  `specs/SPEC-028-native-runtime-shell.md` e `REPOSITORY_TREE.txt`

## Testes executados

- `cmake --preset dev && cmake --build --preset dev`
- `ctest --test-dir build/dev --output-on-failure` — 13/13 aprovados.
- `cmake --install build/dev --prefix build/release`
- `cpack --config build/dev/CPackConfig.cmake -B build/package`
- Inspeção do ZIP: somente `bin/eu_digital_runtime`.
- Smoke `--run` e `--replay` do binário instalado no Linux/WSL.
- `python tools/validate_hybrid.py` — 161 testes Python e 13 CTest naquele
  gate aprovados; release sem runtime Python.
- Helper Windows nativo — MSVC 19.51, SQLite x64 via vcpkg e 14/14 CTest
  aprovados.
- Validação documental, teste documental e árvore do repositório — aprovados.
- O helper Windows foi validado quanto à sintaxe, configuração, compilação e
  execução nativa.

## Resultados

O runtime inicia sem Python, sensores, modelo, rede, UI ou ações. Sem
capacidades opcionais ele fica `ready`; capacidades opcionais não instaladas
ficam explícitas em `RuntimeHealth` como `temporarily_unavailable` e levam a
`degraded`, sem criar observação. Eventos canônicos são preservados como
payload da timeline e o reinício recupera o histórico append-only.

## Critérios de aceite

Todos os critérios de aceite foram verificados no Linux/WSL e no Windows
nativo. A SPEC pode ser considerada concluída.

## Desvios

Não foram implementados sensores, armazenamento de produto, privacidade,
modelo, interface, avatar, sugestões, MSIX ou ações. Esses itens pertencem a
SPECs posteriores do plano de releases e não são antecipados pela SPEC-028.

## Riscos e pendências

- O ambiente Windows possui MSVC 19.51, CMake 4.4.1 e Ninja 1.13.2. O SQLite
  `x64-windows` foi instalado localmente via vcpkg em `C:\src\vcpkg`; o helper
  propagou o ambiente, compilou e executou o gate nativo com sucesso. O
  certificado MSIX não é parte desta SPEC.
- O pacote CPack desta fase permanece ZIP nativo. MSIX assinado é escopo de
  SPEC-044.

## Decisões tomadas

- O host é infraestrutura operacional, não promoção cognitiva: não registra
  componente cognitivo como implantado sem um `promotion_id` aprovado.
- Ausência de capacidade é estado explícito de saúde, não dado de observação.
- O snapshot operacional usa relógio fornecido pela configuração para replay
  verificável, sem depender do relógio de parede.

## Evidências

- `RuntimeHealth` emitido pelo smoke local validou o schema `1.0` com evento
  persistido e, em replay, `recovered_events: 1`.
- O ZIP gerado em `build/package` listou apenas o diretório `bin` e o binário
  `eu_digital_runtime`.
