# Relatório de Execução

SPEC: SPEC-025
Agente: Codex
Data: 2026-07-28
Commit: trabalho em `spec/025-hybrid-monorepo`, ainda não commitado

## Alterações realizadas

- adicionado `pyproject.toml` e `uv.lock` para o pacote isolado do Laboratório;
- criado leitor Python de fixture sem duplicar o schema compartilhado;
- criado schema executável de `CanonicalEvent` em `contracts/schemas/`;
- adicionada fixture compartilhada em `contracts/fixtures/`;
- criado CMake mínimo com Presets, C++23, CTest, install e CPack ZIP;
- criado executável C++ que lê a mesma fixture canônica;
- criado comando unificado `tools/validate_hybrid.py`;
- criada CI híbrida para Python, contratos e C++;
- adicionados testes de instalação isolada e rejeição de contrato incompatível;
- atualizadas as documentações de laboratório, runtime, contratos e comandos.

## Arquivos modificados

- `pyproject.toml`, `uv.lock`;
- `CMakeLists.txt`, `CMakePresets.json`, `cpp/app/main.cpp`;
- `contracts/schemas/canonical_event.schema.json`;
- `contracts/fixtures/canonical_event.json`;
- `python/eu_digital_lab/fixture_reader.py`;
- `python/tests/test_foundation.py`;
- `tools/validate_contracts.py`, `tools/validate_hybrid.py`;
- `.github/workflows/hybrid.yml`;
- `README.md`, `cpp/README.md`, `python/README.md`, `contracts/README.md`;
- `docs/03-contracts/EVENT_SCHEMA.md`;
- `docs/06-operations/DEVELOPMENT_COMMANDS.md`;
- este relatório.

## Testes executados

```powershell
uv lock --check
uv sync --locked
uv run python -m unittest discover -s python/tests -v
uv run python tools/validate_contracts.py
uv run python tools/validate_hybrid.py
```

## Resultados

- 12/12 testes Python aprovados;
- pacote `eu-digital-lab` instalado em ambiente isolado;
- CMake 4.4 configurou o preset `dev`;
- Ninja compilou o executável C++23;
- CTest: 1/1 teste aprovado;
- instalação de release concluída sem arquivos Python;
- fixture canônica lida por Python e C++;
- fixture incompatível rejeitada pelo teste de contrato;
- documentação e árvore do repositório validadas.

## Critérios de aceite

- [x] `cmake --preset dev` configura o Cérebro Implantado.
- [x] `cmake --build --preset dev` produz executável mínimo.
- [x] o pacote Python instala em ambiente isolado.
- [x] Python e C++ leem a mesma fixture de `CanonicalEvent`.
- [x] alteração incompatível em contrato falha em CI.
- [x] pacote de release não contém Python.
- [x] documentação identifica claramente laboratório e runtime.

## Desvios

Nenhum desvio da SPEC. O host não possuía CMake/Ninja/compiler; CMake e Ninja
foram instalados por escopo de usuário e LLVM-MinGW foi usado para a validação
local. A CI usa o ambiente MSVC do Windows.

## Riscos e pendências

- o executável é somente um leitor de fixture; runtime cognitivo, sensores e
  plugins continuam fora da SPEC-025;
- CPack foi configurado, mas a política completa de release pertence às SPECs
  posteriores.

## Decisões tomadas

- manter o schema executável em `contracts/` como fonte compartilhada;
- manter o C++ sem interpretador Python e sem dependências do laboratório;
- usar Ninja no preset local e MSVC na CI via ambiente preparado;
- fazer a validação de contrato incompatível no lado Python, bloqueando CI.

## Rollback

Reverter o commit da SPEC-025 remove a fundação híbrida e preserva SPEC-001 e
SPEC-017 no histórico. Artefatos de build permanecem ignorados pelo Git.

## Evidências

- dependências satisfeitas: SPEC-001 (`abe44ad`);
- branch: `spec/025-hybrid-monorepo`;
- CMake preset: `dev`;
- CTest: 1/1;
- testes Python: 12/12;
- release: `build/release/bin/eu_digital_runtime.exe`, sem `.py`.
