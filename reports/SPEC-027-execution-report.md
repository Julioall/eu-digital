# Relatório de Execução

SPEC: SPEC-027  
Agente: Codex  
Data: 2026-07-28  
Commit: trabalho local não commitado

## Alterações realizadas

- implementado protocolo de validação congelável com revisão independente;
- implementados gates independentes para equivalência, ground truth, holdout,
  metamorfismo, replay, falhas, exportação e sessão online;
- adicionada validação de evidência mínima por gate, impedindo resultados
  positivos sem hash, verdade conhecida ou replay controlado;
- adicionados jitter e cenários de falha determinísticos;
- adicionada comparação entre backends/hardware;
- adicionada auditoria de exportação, quantização, hashes e diferenças de
  acurácia;
- adicionados relatórios longitudinais com métricas cognitivas e operacionais
  separadas.

## Arquivos modificados

- `python/eu_digital_lab/validation.py`;
- `python/eu_digital_lab/__init__.py`;
- `python/tests/test_validation.py`;
- `python/README.md`;
- `docs/06-operations/DEVELOPMENT_COMMANDS.md`;
- `docs/07-research/FINAL_VALIDATION_CHECKLIST.md`;
- `specs/SPEC-027-verification-validation-ecological-gates.md`.

## Testes executados

```text
PYTHONPATH=python python3 -m unittest python.tests.test_validation -q
PYTHONPATH=python python3 -m unittest discover -s python/tests -q
python3 -m compileall -q python
ruff check python/eu_digital_lab/validation.py python/tests/test_validation.py python/eu_digital_lab/__init__.py
ruff format --check python/eu_digital_lab/validation.py python/tests/test_validation.py python/eu_digital_lab/__init__.py
mypy python/eu_digital_lab/validation.py python/tests/test_validation.py
PYTHONPATH=python python3 tools/validate_contracts.py
PYTHONPATH=python python3 tools/validate_sandbox.py datasets/synthetic/v1
```

## Resultados

- 11 testes específicos da SPEC-027 aprovados;
- suíte Python completa aprovada;
- compilação, lint e formatação direcionados aprovados;
- mypy aprovado nos arquivos da SPEC-027;
- contratos compartilhados e corpus sintético validados.

## Critérios de aceite

- [x] equivalência Python–C++ não é usada como único critério;
- [x] fixture com verdade conhecida é exigida como evidência;
- [x] holdout com hash e acesso registrado é exigido;
- [x] mutação metamórfica deliberada é exigida;
- [x] replay controla relógio e ordem;
- [x] falhas são reproduzíveis;
- [x] exportação/quantização gera relatório de diferenças;
- [x] sessão online só ocorre após replay aprovado;
- [x] validade cognitiva e desempenho operacional permanecem separados.

## Desvios

Os gates são infraestrutura de laboratório e não autorizam claims cognitivos
automaticamente. A sessão online e o relatório longitudinal têm interfaces
determinísticas para receber dados reais posteriormente; nenhum sensor ou
serviço externo foi adicionado.

## Riscos e pendências

- a validade ecológica de um componente concreto ainda requer sessões online e
  longitudinais reais;
- auditoria de exportação registra diferenças, mas não promove modelo sem
  protocolo e evidência específicos;
- lint global possui problemas preexistentes fora deste incremento.

## Decisões tomadas

- gates faltantes ou com evidência incompleta falham;
- protocolo congelado não pode ser alterado silenciosamente;
- equivalência computacional é sempre uma evidência entre outras;
- relatórios mantêm classes cognitivas e operacionais em campos distintos.

## Evidências

- implementação: `python/eu_digital_lab/validation.py`;
- testes: `python/tests/test_validation.py`;
- política: `docs/07-research/FINAL_VALIDATION_CHECKLIST.md`;
- SPEC: `specs/SPEC-027-verification-validation-ecological-gates.md`.
