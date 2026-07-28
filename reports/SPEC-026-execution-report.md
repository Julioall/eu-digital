# Relatório de Execução

SPEC: SPEC-026  
Agente: Codex  
Data: 2026-07-28  
Commit: trabalho local não commitado

## Alterações realizadas

- implementado manifesto validável e congelamento de referência;
- implementado gerador de fixtures JSON-lines canônicas e runner Python;
- implementado runner nativo C++ que preserva os bytes de entrada;
- implementado comparador semântico com tolerância numérica e persistência de
  divergências;
- implementado gate de performance e relatório com hipótese, commits, dataset,
  hardware e métricas;
- implementada política de revisão para alterações de tolerância;
- implementado registry persistente de promoções aprovadas em Python e consulta
  correspondente no runtime C++;
- adicionado gate de CI para componentes obrigatórios sem promoção aprovada.

## Arquivos modificados

- `python/eu_digital_lab/promotion.py`;
- `python/tests/test_promotion.py`;
- `tools/check_promotions.py`;
- `promotions/required_components.json`;
- `promotions/registry.json`;
- `cpp/core/promotion_registry.hpp`;
- `cpp/app/promotion_fixture_runner.cpp`;
- `cpp/tests/promotion_registry_test.cpp`;
- `CMakeLists.txt`;
- `.github/workflows/hybrid.yml`;
- documentação em `python/`, `cpp/`, `contracts/`, `docs/` e `specs/`.

## Testes executados

```text
PYTHONPATH=python python3 -m unittest python.tests.test_promotion -q
PYTHONPATH=python python3 -m unittest discover -s python/tests -q
ruff check python/eu_digital_lab/promotion.py python/tests/test_promotion.py tools/check_promotions.py
ruff format --check python/eu_digital_lab/promotion.py python/tests/test_promotion.py tools/check_promotions.py
mypy python/eu_digital_lab/promotion.py python/tests/test_promotion.py tools/check_promotions.py
PYTHONPATH=python python3 tools/check_promotions.py
PYTHONPATH=python python3 tools/check_promotions.py --component missing.component  # esperado: exit 1
cmake -S . -B build/spec026-isolated -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build/spec026-isolated
ctest --test-dir build/spec026-isolated --output-on-failure
promotion_fixture_runner preservando bytes da fixture via cmp
```

## Resultados

- 8 testes específicos da SPEC-026 aprovados;
- suíte Python completa aprovada: 49 testes;
- lint/formatação direcionados aprovados;
- mypy aprovado nos 3 arquivos da SPEC-026;
- gate vazio aprovado e gate de componente não aprovado retornou exit 1;
- build C++ aprovado;
- CTest: 4/4 testes aprovados;
- runner nativo preservou bytes da fixture.

## Critérios de aceite

- [x] referência e candidato recebem bytes semanticamente idênticos;
- [x] todas as divergências são persistidas;
- [x] alteração de tolerância exige justificativa e nova revisão;
- [x] CI bloqueia componente sem promoção aprovada;
- [x] relatório associa hipótese, commits, dataset, hardware e métricas;
- [x] runtime informa a promoção correspondente de cada componente cognitivo.

## Desvios

Nenhum mecanismo cognitivo foi promovido. O runner C++ é um transporte de
fixture determinístico para validar a fronteira e o registry, conforme o
escopo negativo da SPEC; mecanismos reais exigem uma SPEC cognitiva, contrato,
ground truth e evidência científica próprios.

## Riscos e pendências

- o registry obrigatório está vazio porque nenhuma implementação cognitiva
  elegível existe no runtime atual;
- a validade científica continua separada da equivalência Python–C++;
- lint global possui problemas preexistentes fora deste incremento.

## Decisões tomadas

- usar JSON-lines canônico, UTF-8 e chaves ordenadas na fronteira;
- persistir divergências mesmo quando a lista está vazia;
- exigir revisão identificada e justificativa para cada mudança de tolerância;
- não adicionar Python ao produto C++.

## Evidências

- contrato: `docs/03-contracts/CROSS_LANGUAGE_EQUIVALENCE_CONTRACT.md`;
- pipeline: `python/eu_digital_lab/promotion.py`;
- gate: `tools/check_promotions.py` e `.github/workflows/hybrid.yml`;
- runtime: `cpp/core/promotion_registry.hpp`;
- testes: `python/tests/test_promotion.py` e `cpp/tests/promotion_registry_test.cpp`.
