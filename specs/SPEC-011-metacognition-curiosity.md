---
id: SPEC-011
title: Metacognição e curiosidade
status: done
phase: 4
dependencies: [SPEC-009, SPEC-010]
adrs: [ADR-0001, ADR-0002, ADR-0005, ADR-0008, ADR-0009, ADR-0010, ADR-0011, ADR-0013]
contracts: [hypothesis.schema.json, metacognitive_assessment.schema.json, curiosity_question.schema.json, curiosity_response.schema.json]
---

# SPEC-011 — Metacognição e curiosidade

Status: concluída
Fase: 4
Dependências: SPEC-009, SPEC-010
ADRs aplicáveis: ADR-0001, ADR-0002, ADR-0005, ADR-0008, ADR-0009, ADR-0010,
ADR-0011, ADR-0013
Contratos: `hypothesis.schema.json`, `metacognitive_assessment.schema.json`,
`curiosity_question.schema.json`, `curiosity_response.schema.json`

## Objetivo
Avaliar hipóteses e gerar perguntas com ganho informacional.

## Requisitos
- Confiança calibrável.
- Evidência favorável e contrária.
- Alternativas.
- Orçamento de interrupção.
- Supressão de perguntas redundantes.
- Registro de resposta e atualização.

## Escopo negativo
Objetivos irrestritos ou busca autônoma externa.

Também não inclui diálogo, LLM obrigatório, envio de mensagens, serviço
externo, ação autônoma, persistência longitudinal ou promoção para C++.

## Hipótese e protocolo

- **Baselines:** `raw_confidence_v0` sem ajuste por outcomes e
  `fixed_gain_v0` sem seleção por ganho;
- **Hipóteses:** calibração por outcomes reduz Brier/ECE e uma política de
  ganho com orçamento reduz perguntas redundantes sem reduzir excessivamente a
  cobertura de perguntas úteis;
- **Métricas:** Brier, ECE, AUROC, risk–coverage, ganho posterior, precisão de
  supressão, perguntas por janela e resposta/uso humano anotado. Métricas
  operacionais não provam validade cognitiva;
- **Ablação:** desativar calibração, selecionar `fixed_gain_v0`, desligar
  cooldown/redundância e variar orçamento;
- **Falsificação:** confiança permanece desacoplada da correção, ou a política
  de ganho não supera os baselines em ganho posterior, redundância e custo de
  interrupção no holdout congelado;
- **Limite:** pergunta é proposta estruturada, não diálogo, objetivo autônomo
  nem evidência de curiosidade humana.

## Critérios de aceite
- [x] Toda pergunta referencia hipótese.
- [x] Pergunta possui estimativa de ganho.
- [x] Correções reduzem repetição.
- [x] Sistema pode decidir permanecer em silêncio.

## Plano de testes

### Unitários

- schema e proveniência de hipóteses, evidências e alternativas;
- atualização da calibração após outcome confirmado/rejeitado;
- referência obrigatória à hipótese, ganho estimado e silêncio;
- orçamento, cooldown, redundância e redução de repetição após correção;
- respostas inconclusivas não contam como prova negativa.

### Científicos

- Brier/ECE/AUROC/risk–coverage e métricas de curiosidade registrados;
- ablação de calibração, política de ganho e supressão pela mesma interface;
- invariante metamórfica de replay determinístico sob os mesmos timestamps e
  outcomes.
