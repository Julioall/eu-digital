---
id: SPEC-010
title: Atenção e workspace global
status: done
phase: 4
dependencies: [SPEC-008, SPEC-009]
adrs: [ADR-0001, ADR-0002, ADR-0005, ADR-0008, ADR-0009, ADR-0010, ADR-0011, ADR-0012]
contracts: [workspace_candidate.schema.json, workspace_item.schema.json, workspace_snapshot.schema.json, workspace_broadcast.schema.json]
---

# SPEC-010 — Atenção e workspace global

Status: done
Fase: 4
Dependências: SPEC-008, SPEC-009
ADRs aplicáveis: ADR-0001, ADR-0002, ADR-0005, ADR-0008, ADR-0009, ADR-0010, ADR-0011, ADR-0012
Contratos: `workspace_candidate.schema.json`, `workspace_item.schema.json`,
`workspace_snapshot.schema.json`, `workspace_broadcast.schema.json`

## Objetivo
Selecionar e integrar um conjunto limitado de informações relevantes.

## Requisitos
- Pontuação de saliência.
- Capacidade limitada.
- Expiração.
- Justificativa de seleção.
- Broadcast interno.
- Estado observável.

## Escopo negativo
Declarações de consciência.

Também não inclui ação, planejamento, diálogo, saliência aprendida, serviço
externo, persistência de longo prazo ou promoção do mecanismo para C++.

## Hipótese e protocolo

- **Baseline:** `fifo_capacity_v0`, que retém candidatos por ordem de admissão
  sem score de saliência;
- **Hipótese:** `observed_weighted_mean_v1`, com capacidade limitada e
  broadcast, melhora precisão de seleção e robustez a ruído em relação ao
  baseline FIFO no holdout anotado;
- **Métricas:** Precision@k, Recall@k, F1 de seleção, churn, ocupação de
  capacidade e latência local. Métricas operacionais não contam como evidência
  cognitiva;
- **Ablação:** remover fatores observados e selecionar `fifo_capacity_v0` pela
  mesma configuração;
- **Falsificação:** o método não supera FIFO no holdout congelado, ou a
  remoção de capacidade/broadcast não altera as medidas relevantes;
- **Evidência:** classe B para workspace limitado; esta referência é
  laboratório Python e não ground truth nem mecanismo promovido.

## Critérios de aceite
- [x] Itens competem por capacidade.
- [x] Seleção é auditável.
- [x] Itens antigos expiram.
- [x] Mudança de prioridade altera o conteúdo.

## Plano de testes

### Unitários

- competição, capacidade, desempate e atualização explícita de prioridade;
- expiração, descarte por limite de recurso e estado observável;
- ausência de fator preservada como ausência, sem score negativo implícito;
- schemas e erros tipados para entradas inválidas.

### Integração

- broadcast `workspace.selection.v1` em `AsyncEventBus` local;
- snapshots determinísticos sob o mesmo relógio e candidatos;
- invariantes metamórficas de ordem de admissão e escala positiva dos pesos.

### Científicos

- metadata de baseline, hipótese, métrica, ablação e falsificação;
- comparação futura contra relevância anotada/holdout, sem reutilizar holdout
  como conjunto de desenvolvimento.
