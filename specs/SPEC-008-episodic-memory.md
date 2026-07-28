---
id: SPEC-008
title: Memória episódica
status: done
phase: 3
dependencies: [SPEC-007]
adrs: []
contracts: [EPISODE_SCHEMA.md]
---

# SPEC-008 — Memória episódica

Status: done
Fase: 3
Dependências: SPEC-007

## Objetivo
Armazenar, recuperar e relacionar episódios.

## Requisitos
- Busca temporal e estrutural.
- Embeddings locais opcionais.
- Relações de similaridade.
- Proveniência.
- Política de consolidação.

## Escopo negativo
Generalização semântica automática.

## Hipótese e protocolo

- **Baseline:** `context_overlap_v1`, com fallback cronológico determinístico;
- **Hipótese:** recuperar por contexto observado e proveniência melhora a
  precisão de recuperação em relação a um log puramente cronológico;
- **Métricas:** Recall@k, MRR, precisão de proveniência e retenção após
  consolidação;
- **Ablação:** remover similaridade/embedding e comparar com o fallback
  temporal;
- **Falsificação:** a recuperação contextual não supera o baseline
  cronológico no holdout anotado;
- **Política:** retenção limitada e explícita; consolidação não cria resumo,
  fato ou generalização semântica.

## Critérios de aceite
- [x] Recupera episódios por contexto.
- [x] Explica por que um episódio foi recuperado.
- [x] Não mistura hipótese com fato.
