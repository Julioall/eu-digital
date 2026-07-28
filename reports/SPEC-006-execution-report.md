# Relatório de Execução

SPEC: SPEC-006
Agente: Codex
Data: 2026-07-28
Commit: trabalho local não commitado

## Alterações realizadas

- implementado `TimelineStore` local baseado em SQLite;
- adicionada migração versionada do schema 0 para o schema 1;
- implementado armazenamento append-only com idempotência por `event_id`;
- adicionados índices temporais e de sessão/aplicativo/correlação;
- implementadas consultas temporais/contextuais paginadas;
- implementados exportação JSON para string/arquivo e replay ordenado;
- adicionada camada de declarações SQLite mínima para ambientes que possuem a
  biblioteca runtime sem `sqlite3.h`;
- adicionada instalação local da dependência SQLite ao workflow Windows;
- adicionados testes determinísticos de reinicialização, migração, ordenação,
  paginação, exportação e replay.

## Arquivos modificados

- `cpp/core/sqlite3_compat.hpp`;
- `cpp/core/timeline_store.hpp`;
- `cpp/tests/timeline_store_test.cpp`;
- `CMakeLists.txt`;
- `.github/workflows/hybrid.yml`;
- `cpp/README.md`;
- `docs/06-operations/DEVELOPMENT_COMMANDS.md`;
- `docs/05-governance/OPEN_QUESTIONS.md`;
- `specs/SPEC-006-timeline-store.md`.

## Testes executados

```text
cmake -S . -B build/spec006-isolated -G Ninja -DCMAKE_CXX_COMPILER=x86_64-linux-gnu-g++-13 -DCMAKE_CXX_FLAGS="--sysroot=/tmp/tmp.21tTxQm2JN/extracted"
cmake --build build/spec006-isolated -j2
ctest --test-dir build/spec006-isolated --output-on-failure
PYTHONPATH=python python3 -m unittest discover -s python/tests -q
PYTHONPATH=python python3 tools/validate_contracts.py
PYTHONPATH=python python3 tools/validate_sandbox.py datasets/synthetic/v1
PYTHONPATH=python python3 tools/validate_hybrid.py --skip-build
/tmp/tmp.HyT0IpIN6S/pkg/ruff-0.16.0.data/scripts/ruff check python/eu_digital_lab/evaluation.py python/eu_digital_lab/promotion.py python/eu_digital_lab/validation.py python/tests/test_evaluation.py python/tests/test_promotion.py python/tests/test_validation.py
PYTHONPATH=/tmp/tmp.21tTxQm2JN/extracted/usr/lib/python3/dist-packages:python /tmp/tmp.21tTxQm2JN/extracted/usr/bin/mypy python/eu_digital_lab
```

## Resultados

- teste específico do timeline aprovado;
- suíte CTest completa: 8 testes aprovados;
- suíte Python: 60 testes aprovados;
- contratos, corpus sintético, validador híbrido, Ruff direcionado e mypy aprovados.

## Critérios de aceite

- [x] reinicialização preserva eventos;
- [x] consulta temporal retorna ordem correta;
- [x] migração é testada;
- [x] replay reproduz sequência determinística.

## Desvios

O `CanonicalEvent` atual mantém o contrato C++ mínimo do runtime; sessão,
aplicativo e correlação são recebidos como `TimelineMetadata` e persistidos ao
lado do evento, sem alterar o contrato público. O armazenamento semântico,
embeddings e consolidação permanecem fora do escopo.

## Riscos e pendências

- o capturador de plataforma deve fornecer metadados de sessão/aplicativo se
  esses filtros forem necessários em produção;
- o ambiente Linux desta execução não possui `sqlite3.h`, portanto o teste usa
  a biblioteca SQLite do sistema com declarações ABI mínimas; Windows CI usa a
  distribuição vcpkg;
- retenção, compactação e esquecimento permanecem responsabilidades das SPECs
  de memória posteriores.

## Decisões tomadas

- usar SQLite local como backend inicial, sem rede;
- ordenar por `monotonic_ns` e usar `sequence` como desempate determinístico;
- rejeitar atualização/remoção pela API pública para preservar append-only;
- retornar duplicata para `event_id` já persistido sem substituir o registro;
- manter exportação e replay como operações explícitas e reproduzíveis.

## Evidências

- implementação: `cpp/core/timeline_store.hpp`;
- compatibilidade SQLite: `cpp/core/sqlite3_compat.hpp`;
- teste: `cpp/tests/timeline_store_test.cpp`;
- CMake/CTest: `CMakeLists.txt`;
- CI de dependência local: `.github/workflows/hybrid.yml`.
