---
id: SPEC-012
title: Modelo de si funcional
status: done
phase: 4
dependencies: [SPEC-010, SPEC-011]
adrs: [ADR-0001, ADR-0005, ADR-0008, ADR-0009, ADR-0010, ADR-0011, ADR-0014]
contracts: [self_model_internal_event.schema.json, functional_self_model_snapshot.schema.json, self_model_decision.schema.json]
---

# SPEC-012 — Modelo de si funcional

Status: concluída
Fase: 4
Dependências: SPEC-010, SPEC-011
ADRs aplicáveis: ADR-0001, ADR-0005, ADR-0008, ADR-0009, ADR-0010, ADR-0011,
ADR-0014
Contratos: `self_model_internal_event.schema.json`,
`functional_self_model_snapshot.schema.json`, `self_model_decision.schema.json`

## Objetivo
Manter representação versionada das capacidades, limitações, estado e história do agente.

## Requisitos
- Atualização por eventos internos.
- Histórico imutável de versões.
- Distinção entre fato, hipótese e configuração.
- Uso causal pelo orquestrador.
- Explicação de limitações.

## Escopo negativo
Sentimentos reais, personalidade fixa ou alegação de subjetividade.

Também não inclui ação, diálogo, LLM, integração de plugin concreto,
persistência longitudinal ou promoção para C++.

## Hipótese e protocolo

- **Baseline:** `unconstrained_decision_v0`, que não consulta snapshot;
- **Hipótese H6:** `self_model_gate_v1` melhora acurácia de capacidade,
  consistência temporal e explicação de limitações, alterando decisões quando
  uma capacidade muda;
- **Métricas:** acurácia de capacidade, consistência de versões, recuperação de
  versão, decisões incompatíveis bloqueadas e cobertura de explicação;
- **Ablação:** selecionar o baseline pela mesma interface e remover o
  self-model da decisão;
- **Falsificação:** a remoção do modelo não altera as decisões relevantes, ou
  snapshots não representam corretamente a disponibilidade declarada;
- **Limite:** efeito causal local não demonstra subjetividade, personalidade,
  consciência ou validade ecológica.

## Critérios de aceite
- [x] Mudança de capacidade atualiza versão.
- [x] O agente explica o que pode e não pode fazer.
- [x] Decisões usam estado do modelo.
- [x] Versão anterior é recuperável.

## Plano de testes

### Unitários

- evento de capacidade cria nova versão e preserva a anterior;
- fatos, hipóteses e configuração não se misturam;
- capacidade disponível, degradada, indisponível, removida e ausente recebem
  explicações distintas;
- decisão bloqueia estado incompatível por `self_model_gate_v1`;
- snapshots, hashes e decisões são determinísticos para os mesmos eventos.

### Científicos

- baseline sem consulta do modelo é selecionável pela mesma interface;
- ablação altera a decisão em cenário de perda de capacidade;
- métricas, hipótese e falsificação permanecem registradas sem alegação de
  validade científica.
