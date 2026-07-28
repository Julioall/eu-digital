# Relatório de Execução

SPEC: SPEC-007
Agente: Codex
Data: 2026-07-28
Commit: trabalho local não commitado

## Alterações realizadas

- adicionada a definição executável compartilhada `episode.schema.json`;
- implementado o baseline Python `time_context_threshold_v1`;
- implementados limites por lacuna temporal e mudanças observadas de
  aplicação/documento;
- adicionadas features explícitas de aplicação, documento, entrada e OCR;
- adicionadas razões estruturadas para cada limite e IDs determinísticos;
- adicionada reexecução offline determinística;
- adicionadas métricas baseline de boundary precision/recall/F1 e WindowDiff;
- registrada hipótese, ablação e critério de falsificação;
- mantida a implementação no laboratório, sem promoção automática ao C++.

## Arquivos modificados

- `contracts/schemas/episode.schema.json`;
- `contracts/README.md`;
- `python/eu_digital_lab/episode_segmentation.py`;
- `python/eu_digital_lab/__init__.py`;
- `python/tests/test_episode_segmentation.py`;
- `docs/03-contracts/EPISODE_SCHEMA.md`;
- `python/README.md`;
- `docs/06-operations/DEVELOPMENT_COMMANDS.md`;
- `specs/SPEC-007-episode-segmentation.md`.

## Testes executados

```text
PYTHONPATH=python python3 -m unittest python.tests.test_episode_segmentation -v
PYTHONPATH=python python3 -m unittest discover -s python/tests -q
PYTHONPATH=python python3 tools/validate_contracts.py
PYTHONPATH=python python3 tools/validate_sandbox.py datasets/synthetic/v1
PYTHONPATH=python python3 tools/validate_hybrid.py --skip-build
/tmp/tmp.HyT0IpIN6S/pkg/ruff-0.16.0.data/scripts/ruff check python/eu_digital_lab/episode_segmentation.py python/tests/test_episode_segmentation.py
PYTHONPATH=/tmp/tmp.21tTxQm2JN/extracted/usr/lib/python3/dist-packages:python /tmp/tmp.21tTxQm2JN/extracted/usr/bin/mypy python/eu_digital_lab
```

## Resultados

- testes específicos: 7 aprovados;
- suíte Python completa: 67 testes aprovados;
- contratos e corpus sintético validados;
- validador híbrido, Ruff direcionado e mypy aprovados.

## Critérios de aceite

- [x] cada limite possui motivo;
- [x] segmentação é determinística com a mesma configuração;
- [x] métrica baseline é registrada.

## Desvios

Esta execução entrega a referência científica Python prevista no fluxo de
implementação. O runtime C++ não recebe o segmentador antes de haver evidência
de baseline, ablação, avaliação anotada e equivalência; isso evita tratar uma
implementação experimental como ground truth.

## Riscos e pendências

- os testes atuais usam sessões determinísticas sintéticas; a avaliação final
  exige sessões anotadas e holdout bloqueado;
- `people`, `topics`, resumo e embeddings permanecem vazios/nulos quando não há
  observação explícita;
- a qualidade emitida pelo baseline é operacional e não constitui evidência
  cognitiva;
- a comparação com modalidades isoladas ainda precisa ser executada no
  protocolo científico antes de qualquer promoção; a remoção das divisões de
  contexto já é exercitada como ablação de engenharia.

## Decisões tomadas

- não inferir mudança quando a observação de contexto está ausente;
- ordenar valores de contexto para estabilidade byte a byte;
- manter razões semânticas simples e auditáveis, sem nomeação por LLM;
- usar IDs UUID5 derivados de sessão, configuração e primeiro evento;
- versionar o contrato em `contracts/` antes de produzir episódios.

## Evidências

- implementação: `python/eu_digital_lab/episode_segmentation.py`;
- contrato: `contracts/schemas/episode.schema.json`;
- testes: `python/tests/test_episode_segmentation.py`;
- SPEC e protocolo: `specs/SPEC-007-episode-segmentation.md`.
