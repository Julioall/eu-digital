---
id: SPEC-009
title: Aprendizagem incremental de padrões
status: done
phase: 4
dependencies: [SPEC-008]
adrs: []
contracts: [pattern.schema.json]
---

# SPEC-009 — Aprendizagem incremental de padrões

Status: done
Fase: 4
Dependências: SPEC-008

## Objetivo
Descobrir recorrências e sequências sem catálogo de tarefas.

## Requisitos
- Clustering incremental.
- Suporte, estabilidade e recência.
- Detecção de mudança de conceito.
- Padrões versionados.
- Feedback positivo e negativo.

## Escopo negativo
Nomeação automática como verdade e execução de ações.

## Hipótese e protocolo

- **Baseline:** `online_exact_threshold_v1`, com distância configurável sobre
  features numéricas observadas;
- **Hipótese:** clusters incrementais com suporte e feedback explícitos
  descobrem recorrências mais estáveis que a correspondência exata;
- **Métricas:** suporte, estabilidade, recência, confiança e false discovery
  rate, além de precisão/recall em sessões anotadas;
- **Ablação:** distância zero e remoção de feedback/similaridade;
- **Falsificação:** o learner incremental não supera o baseline de chave exata
  no holdout, ou deriva não é detectada por nova versão/divisão;
- **Limite:** promoção significa apenas estado operacional do padrão; não é
  nomeação, hipótese confirmada ou ação executável.

## Critérios de aceite
- [x] Padrão só é promovido após suporte configurável.
- [x] Correção humana altera confiança.
- [x] Deriva gera nova versão ou divisão.
- [x] Métricas de cluster são registradas.
