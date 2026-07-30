---
id: SPEC-035
title: Promoção nativa da aprendizagem incremental de padrões
status: done
phase: beta
dependencies: [SPEC-009, SPEC-023, SPEC-026, SPEC-027, SPEC-029, SPEC-034]
adrs: [ADR-0005, ADR-0008, ADR-0009, ADR-0010, ADR-0011, ADR-0031]
contracts: [PATTERN_SCHEMA.md, PATTERN_PROMOTION_CONTRACT.md]
---

# SPEC-035 — Promoção nativa da aprendizagem incremental de padrões

## Objetivo

Promover o learner incremental de padrões para C++ nativo, preservando clusters
versionados, suporte, estabilidade, recência, confiança, feedback humano e
detecção de drift sem catálogo fixo de tarefas.

## Escopo negativo

Não nomear automaticamente um padrão como verdade, não criar fatos ou resumo,
não executar ações, não planejar tarefas, não integrar modelo, diálogo, avatar,
world model, workspace, self-model ou metacognição.

## Escopo

Inclui distância configurável sobre features numéricas observadas, promoção por
suporte/confiança, feedback positivo/negativo, IDs determinísticos, versões de
drift, snapshot e métricas operacionais. O componente é removível via
`CapabilityDescriptor`.

## Dependências e decisões

- `SPEC-009` é a referência Python congelada.
- `SPEC-034` fornece a memória episódica, mas esta SPEC recebe observações já
  extraídas e não altera episódios.
- `SPEC-026`/`SPEC-027` exigem baseline exato, ablação, holdout e separação
  entre equivalência e validade científica.
- `ADR-0031` torna a promoção atômica e mantém o produto indisponível sem
  aprovação no registry.

## Critérios de aceite

- [x] Referência, fixtures e holdout possuem hashes registrados antes da
      avaliação final.
- [x] Distância, suporte, confiança, feedback, centroid e status reproduzem o
      contrato `pattern.schema.json`.
- [x] Feedback preserva referências e pode retirar o estado `promoted` sem
      apagar observações.
- [x] Drift cria nova versão, preserva o parent e marca o anterior como
      `superseded`.
- [x] Baseline de chave exata, ablação sem clustering cruzado, invariantes e
      benchmark p50/p95/máximo estão registrados.
- [x] Candidato e plugin são removíveis; nenhum padrão vira ação ou fato.
- [x] CTest, suíte Python, equivalência, holdout, contratos, documentação e
      relatório passam em Linux e Windows.

## Saída

Um candidato C++ equivalente e auditável. O componente permanece
`product_status: unavailable` até revisão humana com identificador no registry.
