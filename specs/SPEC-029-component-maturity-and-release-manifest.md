---
id: SPEC-029
title: Maturidade de componentes e registro de release
status: done
phase: 0.3
dependencies: [SPEC-025, SPEC-026, SPEC-027]
adrs: [ADR-0005, ADR-0010, ADR-0025]
contracts: [component_maturity.schema.json]
---

# SPEC-029 — Maturidade de componentes e registro de release

Status: done
Fase: 0.3
Dependências: SPEC-025, SPEC-026, SPEC-027
ADRs aplicáveis: ADR-0005, ADR-0010, ADR-0025
Contrato aplicável: `component_maturity.schema.json`

## Objetivo

Separar o estado documental das SPECs, a maturidade da referência Python, a
promoção nativa C++ e a disponibilidade no produto. Nenhum componente será
considerado distribuível apenas porque sua SPEC possui status `done`.

## Entregáveis

- schema versionado de maturidade de componentes;
- registro local separado do registro de promoções;
- validador determinístico com referências de SPEC e evidências locais
  resolvíveis;
- fixture válida e fixture inválida;
- teste de regras de promoção e disponibilidade;
- documentação da distinção entre SPEC, referência, nativo e produto.

## Escopo negativo

- não promover mecanismos cognitivos;
- não alterar o status histórico das SPECs existentes;
- não habilitar sensores, modelo, interface, avatar ou ações;
- não substituir `promotions/registry.json`;
- não declarar validade científica a partir de maturidade operacional.

## Critérios de aceite

- [x] O contrato define `reference_status`, `native_status` e
      `product_status` separadamente.
- [x] Registro válido é carregado e referências de SPEC são resolvidas.
- [x] Evidências são locais, existentes e não escapam da raiz do repositório.
- [x] IDs duplicados e estados incompatíveis são rejeitados.
- [x] `beta`/`released` exigem nativo promovido e `promotion_id`.
- [x] O registro não altera nem substitui o status documental das SPECs.
- [x] Fixture inválida e regras de promoção possuem testes.
- [x] Documentação operacional e árvore do repositório estão atualizadas.

## Protocolo operacional

Hipótese: estados separados reduzem falsos positivos de disponibilidade de
produto causados por interpretar `done` documental como promoção nativa.

- baseline: leitura de SPECs e do registro de promoções sem estado de produto;
- métrica: estados incompatíveis detectados e referências não resolvidas;
- ablação: remover a validação de `native_status` antes de aceitar `released`;
- falsificação: o registro aceitar produto released sem promoção, duplicar
  componente ou resolver uma SPEC inexistente.

Essas são métricas de governança operacional, não evidência cognitiva.

## Dependências e bloqueios

O registro é independente de sensores e modelos. A conclusão do Runtime
Preview continua sujeita ao smoke test Windows da SPEC-028; esta SPEC não o
ignora nem muda seus critérios.
