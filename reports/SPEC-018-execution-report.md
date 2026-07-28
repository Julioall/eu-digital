# Relatório de Execução

SPEC: SPEC-018  
Agente: Codex  
Data: 2026-07-28  
Commit: trabalho local não commitado

## Alterações realizadas

- implementado harness Python local e sem dependências externas;
- adicionados configuração de experimento, flags de ablação, seeds e
  proveniência de commit/hardware/backend;
- adicionados coleta de duração/memória, métricas cognitivas e operacionais
  separadas, comparação baseline/tratamento e intervalos de incerteza;
- adicionados repositório de datasets com hashes e holdout bloqueado;
- adicionados ground truth, replay com relógio virtual, injeção de falhas e
  verificação metamórfica;
- adicionados testes unitários, exportação do pacote e documentação operacional.

## Arquivos modificados

- `python/eu_digital_lab/evaluation.py`;
- `python/eu_digital_lab/__init__.py`;
- `python/tests/test_evaluation.py`;
- `python/README.md`;
- `docs/06-operations/DEVELOPMENT_COMMANDS.md`;
- `specs/SPEC-018-scientific-evaluation-harness.md`.

## Testes executados

```text
PYTHONPATH=python python3 -m unittest discover -s python/tests -q
python3 -m compileall -q python
ruff check python/eu_digital_lab/evaluation.py python/tests/test_evaluation.py python/eu_digital_lab/__init__.py
ruff format --check python/eu_digital_lab/evaluation.py python/tests/test_evaluation.py python/eu_digital_lab/__init__.py
mypy python/eu_digital_lab/evaluation.py python/tests/test_evaluation.py
PYTHONPATH=python python3 tools/validate_contracts.py
PYTHONPATH=python python3 tools/validate_sandbox.py datasets/synthetic/v1
cmake -S . -B build/spec018-isolated -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build/spec018-isolated
ctest --test-dir build/spec018-isolated --output-on-failure
PYTHONPATH=python python3 tools/validate_hybrid.py --skip-build
```

## Resultados

- 41 testes Python aprovados;
- compilação Python aprovada;
- lint e formatação direcionados aprovados;
- mypy aprovado nos arquivos da SPEC-018;
- 8 módulos Python de produção aprovados no mypy;
- contratos compartilhados validados;
- corpus sintético validado: 6 sessões e 6 arquivos.
- build C++ e CTest: 3/3 testes aprovados;
- fluxo híbrido sem build repetido aprovado.

## Critérios de aceite

- [x] módulo pode ser desativado sem alteração de código;
- [x] experimento registra commit, hardware, backend e configuração;
- [x] relatório compara baseline e tratamento;
- [x] pelo menos uma condição utiliza ground truth;
- [x] teste metamórfico detecta mutação conhecida;
- [x] holdout possui hash e política de acesso;
- [x] resultados cognitivos e operacionais são apresentados separadamente.

## Desvios

O harness é uma implementação de laboratório Python e não altera o runtime
C++. A coleta de hardware usa somente informações locais da plataforma; não há
telemetria externa. O holdout só é liberado para o propósito explícito
`final-evaluation`, com acesso registrado.

## Riscos e pendências

- validade ecológica e estudos longitudinais permanecem fora desta SPEC;
- o lint global ainda possui problemas preexistentes fora do escopo desta
  alteração (`annotation.py`, `event_bus.py`, `fixture_reader.py`, `sandbox.py`,
  testes e ferramentas);
- a referência Python não é ground truth científico.

## Decisões tomadas

- manter o harness sem dependências de terceiros para preservar execução local;
- representar métricas como amostras para permitir médias e intervalos de
  incerteza sem declarar significância estatística indevida;
- tratar falha injetada como resultado registrado do trial, permitindo comparar
  robustez sem interromper a execução do tratamento oposto.

## Evidências

- implementação: `python/eu_digital_lab/evaluation.py`;
- testes: `python/tests/test_evaluation.py`;
- dataset/holdout: `datasets/synthetic/v1/manifest.json`;
- política: `docs/07-research/SCIENTIFIC_DECISION_POLICY.md` e ADR-0011.
