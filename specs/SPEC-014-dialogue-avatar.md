---
id: SPEC-014
title: Diálogo e avatar
status: done
phase: 5
dependencies: [SPEC-011, SPEC-012, SPEC-013]
adrs: [ADR-0001, ADR-0002, ADR-0004, ADR-0005, ADR-0008, ADR-0013, ADR-0016]
contracts: [dialogue_notice.schema.json, dialogue_feedback.schema.json, avatar_view_state.schema.json]
---

# SPEC-014 — Diálogo e avatar

Status: concluída
Fase: 5
Dependências: SPEC-011, SPEC-012, SPEC-013
ADRs aplicáveis: ADR-0001, ADR-0002, ADR-0004, ADR-0005, ADR-0008, ADR-0013,
ADR-0016
Contratos: `dialogue_notice.schema.json`, `dialogue_feedback.schema.json`,
`avatar_view_state.schema.json`

## Objetivo
Fornecer presença visual discreta e diálogo contextual.

## Requisitos
- Avatar desktop.
- Estados visuais limitados e não enganosos.
- Perguntas, notificações e histórico.
- Controle de interrupções.
- Explicação de hipótese e confiança.

## Escopo negativo
Antropomorfismo que declare sentimentos ou consciência.

Também não inclui escolha de framework desktop, personalidade, ação,
persistência de longo prazo ou promoção para C++.

## Hipótese e protocolo

- **Baseline:** presenter desabilitado, mantendo os mesmos notices e feedback;
- **Hipótese:** apresentação contextual não bloqueante melhora a compreensão
  do motivo da pergunta sem aumentar interrupções;
- **Métricas:** cobertura de contexto/motivo, correção registrada, adiamento,
  silêncio, interrupções e invariantes de não bloqueio;
- **Ablação:** remover presenter e comparar pela mesma interface;
- **Falsificação:** presenter captura foco/input, bloqueia trabalho ou feedback
  não controla a interrupção;
- **Limite:** estado visual é representação operacional, não emoção, intenção
  ou consciência.

## Critérios de aceite
- [x] Avatar não bloqueia o trabalho.
- [x] Pergunta mostra contexto e motivo.
- [x] Usuário pode corrigir, adiar ou silenciar.

## Plano de testes

- view state valida invariantes de não bloqueio, foco e captura;
- notice preserva hipótese, confiança, contexto e motivo;
- controller registra notificação, pergunta e histórico;
- orçamento limita interrupções;
- `correct`, `defer` e `silence` têm transições auditáveis;
- replay é determinístico e não importa framework ou LLM.
