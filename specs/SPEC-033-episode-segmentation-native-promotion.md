---
id: SPEC-033
title: Promoção nativa da segmentação de episódios
status: in_progress
phase: beta
dependencies: [SPEC-007, SPEC-023, SPEC-026, SPEC-027, SPEC-029]
adrs: [ADR-0005, ADR-0008, ADR-0009, ADR-0010, ADR-0011, ADR-0029]
contracts: [EPISODE_SCHEMA.md]
---

# SPEC-033 — Promoção nativa da segmentação de episódios

## Objetivo

Promover a segmentação determinística de episódios do laboratório Python para
uma implementação C++ nativa verificável, sem torná-la ainda uma capacidade
disponível no produto. Esta SPEC cobre exatamente um mecanismo cognitivo.

## Escopo negativo

Não inclui memória episódica, padrões, world model, workspace, self-model,
metacognição, modelo local, diálogo, avatar, sensores ou ações.

## Escopo

Inclui a referência congelada, o candidato C++, equivalência de fixtures,
ground truth anotado, baseline temporal, ablação de contexto, holdout bloqueado,
testes metamórficos/invariantes, benchmark operacional e manifesto de promoção.

## Dependências e decisões

- `SPEC-007` define a hipótese, baseline e contrato de episódio do laboratório.
- `SPEC-023` exige `CapabilityDescriptor`, remoção e substituição sem dependência
  estrutural no núcleo.
- `SPEC-026` e `SPEC-027` exigem evidência separada de equivalência e validade
  científica; concordância Python/C++ não é ground truth.
- `SPEC-029` separa `reference_status`, `native_status` e `product_status`.
- `ADR-0029` decide que a promoção é atômica e que o registro de aprovação não é
  alterado automaticamente por uma execução local.

## Critérios de aceite

- [ ] A referência Python e o conjunto de equivalência possuem hash registrado.
- [ ] O candidato C++ produz o contrato `episode.schema.json` completo e IDs
      determinísticos para as mesmas entradas.
- [ ] A equivalência usa bytes de entrada idênticos e não contém divergências.
- [ ] Existem casos para mudança de aplicação/documento, gap temporal, contexto
      ausente e ablação de contexto.
- [ ] Há ground truth/invariantes e holdout separado, bloqueado antes da avaliação.
- [ ] Os testes verificam cobertura de eventos, ausência de episódios vazios,
      explicação das fronteiras e ausência de observação não tratada como negativa.
- [ ] O benchmark reporta mediana, p95 e máximo, sem apresentá-los como evidência
      cognitiva.
- [ ] O registro de maturidade não declara disponibilidade de produto; a aprovação
      em `promotions/registry.json` requer revisão humana identificável.
- [ ] CTest, validação Python, contratos, documentação e relatório passam.

## Saída

Um candidato nativo equivalente e auditável, disponível para uma futura decisão
de promoção. O runtime continua `degraded`/sem a capacidade enquanto não houver
entrada aprovada no registro de promoções.
