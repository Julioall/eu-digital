# Relatório de Execução

SPEC: SPEC-030 — Privacidade, consentimento e armazenamento local
Agente: Codex
Data: 2026-07-29
Commit: pendente de criação

## Alterações realizadas

- Criados contratos versionados de consentimento, armazenamento, saúde e
  gestão de dados.
- Implementado ledger C++ de concessão/revogação por sensor e finalidade,
  negação padrão e pausa global.
- Implementado controller de quota com defaults 30/365/14 dias e 10 GiB,
  buckets locais explícitos, contabilização separada de modelo e suspensão
  sem exclusão automática.
- Implementadas operações locais de exportação, exclusão confirmada e
  recuperação com cópia verificada e quarentena do arquivo substituído.
- Implementada proteção de snapshots de consentimento com Windows DPAPI;
  Linux/WSL reporta indisponibilidade sem fallback plaintext.
- Exposto `StorageHealth` separado, preservando `RuntimeHealth` 1.0.
- Adicionadas referência Python, fixtures, testes C++ e documentação normativa.

## Arquivos modificados

- `contracts/schemas/consent_policy.schema.json`
- `contracts/schemas/storage_policy.schema.json`
- `contracts/schemas/storage_health.schema.json`
- `contracts/schemas/data_management_request.schema.json`
- `contracts/fixtures/consent_policy*.json`
- `contracts/fixtures/storage_policy*.json`
- `contracts/fixtures/storage_health.json`
- `contracts/fixtures/data_management_request*.json`
- `cpp/core/privacy_storage.hpp`
- `cpp/core/runtime_host.hpp`
- `cpp/tests/privacy_storage_test.cpp`
- `cpp/tests/runtime_host_test.cpp`
- `python/eu_digital_lab/privacy_storage.py`
- `python/tests/test_privacy_storage.py`
- `tools/validate_privacy_storage.py`
- `CMakeLists.txt`
- `docs/03-contracts/PRIVACY_STORAGE_CONTRACTS.md`
- `docs/04-adrs/ADR-0026-local-consent-and-storage-protection.md`
- `specs/SPEC-030-privacy-consent-and-local-storage.md`
- `docs/06-operations/DEVELOPMENT_COMMANDS.md`
- `contracts/README.md`
- `REPOSITORY_TREE.txt`

## Testes executados

- `python tools/validate_privacy_storage.py` — aprovado.
- `python -m unittest python.tests.test_privacy_storage -v` — 7/7.
- `python tools/validate_hybrid.py` — 173 testes Python e 14 CTest.
- `ctest --test-dir build/dev --output-on-failure` — 14/14.
- Helper Windows nativo — compilação, link DPAPI e 14/14 CTest aprovados.
- Instalação CMake e verificação de release sem runtime Python — aprovadas.
- Validação documental, contratos e árvore — aprovadas.

## Resultados

Ausência de consentimento, revogação e pausa global bloqueiam captura. Quota
excedida deixa o armazenamento `degraded`, suspende novas capturas e exige
decisão do usuário sem apagar dados. Exportação e exclusão não confirmadas são
rejeitadas. Recuperação preserva o arquivo anterior como `.corrupt` e troca
somente após verificar a cópia de backup.

## Critérios de aceite

Todos os critérios locais, de contrato e Windows foram atendidos. O helper
nativo comprovou compilação, link com `Crypt32`, round-trip DPAPI e 14/14 CTest;
a SPEC pode ser considerada concluída.

## Desvios

Não foram integrados sensores reais, OCR, modelo, interface, avatar, ações ou
rede. O formato público de `CanonicalEvent` e `RuntimeHealth` 1.0 não foi
alterado.

## Riscos e pendências

- O código Windows DPAPI foi condicionado a `_WIN32` e foi comprovado no smoke
  test nativo Windows com round-trip de proteção.
- A captura de sensores e a aplicação de consentimento nos adaptadores são
  escopo das SPECs 031 e 032; esta fase fornece a porta e a política.

## Decisões tomadas

- Não existe fallback criptográfico silencioso fora do Windows.
- `StorageHealth` é um contrato separado para não quebrar consumidores de
  `RuntimeHealth` 1.0.
- Quota usa reserva prévia e nova medição após commit; a transação SQLite não
  é desfeita por falta de espaço detectada depois da gravação.

## Evidências

- `valid privacy and storage contracts`.
- `Ran 7 tests ... OK` na suíte específica.
- `100% tests passed out of 14` no CTest.
- `Ran 173 tests ... OK` e `hybrid validation passed; release contains no Python runtime files`.
