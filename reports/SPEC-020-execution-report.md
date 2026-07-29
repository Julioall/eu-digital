# Relatório de Execução

SPEC: SPEC-020 — Consolidação e esquecimento
Agente: Codex
Data: 2026-07-29
Commit: feat: complete SPEC-020 memory consolidation

## Alterações realizadas

O bloqueio documental foi resolvido com ADR-0020 e três contratos. A
referência Python implementa replay local de contexto observável,
SemanticKnowledge com proveniência, reconciliação de versões e alternativas,
baseline sem replay e retenção por archive/restore reversível.

## Arquivos modificados

- python/eu_digital_lab/memory_consolidation.py
- python/tests/test_memory_consolidation.py
- python/eu_digital_lab/__init__.py
- contracts/schemas/semantic_knowledge.schema.json
- contracts/schemas/consolidation_record.schema.json
- contracts/schemas/retention_decision.schema.json
- docs/03-contracts/MEMORY_CONSOLIDATION_SCHEMA.md
- docs/04-adrs/ADR-0020-reversible-memory-consolidation-retention.md
- docs/02-architecture/COMPONENT_CATALOG.md
- docs/05-governance/OPEN_QUESTIONS.md
- docs/06-operations/DEVELOPMENT_COMMANDS.md
- contracts/README.md
- specs/SPEC-020-memory-consolidation-forgetting.md

## Testes executados

- validação JSON dos três schemas
- python tools/validate_contracts.py
- Ruff e mypy nos arquivos novos
- python tools/validate_hybrid.py

## Resultados

- Schemas e contrato canônico: passaram.
- Ruff e mypy dos arquivos novos: passaram.
- Testes Python: 144 passaram.
- CTest: 10/10 passaram.
- Build e instalação híbridos: passaram; release sem Python.
- Lint e mypy globais continuam com violações preexistentes registradas nas
  fases anteriores; nenhum erro novo foi introduzido pela SPEC-020.
- Validadores documentais PowerShell não executados porque pwsh não está
  instalado neste ambiente.

## Critérios de aceite

- [x] Replay reduz esquecimento no corpus: tratamento cria conhecimento e
  supera o baseline no_replay_v0 em retention_score.
- [x] Conhecimento consolidado aponta para episódios por source_episode_ids.
- [x] Archive e restore são reversíveis em teste e não apagam episódios.

## Desvios

Nenhum desvio funcional. A generalização fica limitada a chaves observáveis de
contexto; não há LLM, síntese sem fonte ou promoção para C++.

## Riscos e pendências

- Retenção é lógica e reversível; remoção física de bytes requer política
  posterior.
- A validade semântica do conhecimento exige ground truth e avaliação
  longitudinal.
- Hipóteses permanecem alternativas/contradições, nunca fatos automáticos.

## Decisões tomadas

- Cada conhecimento guarda fontes, suporte, versão e custo de replay.
- Reconciliação acumula fontes e alternativas em novos replay.
- O baseline usa a mesma interface e não cria conhecimento.

## Evidências

- ADR: docs/04-adrs/ADR-0020-reversible-memory-consolidation-retention.md.
- Contratos: contracts/schemas/semantic_knowledge.schema.json,
  consolidation_record.schema.json e retention_decision.schema.json.
- Teste: python/tests/test_memory_consolidation.py.
