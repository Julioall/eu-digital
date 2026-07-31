---
id: SPEC-053
title: "Companheiro Local de Atividades e Assistência Contextual"
status: draft
phase: product_beta
dependencies: ["SPEC-045", "SPEC-030", "SPEC-047"]
adrs: []
contracts: []
---

# SPEC-053 — Companheiro Local de Atividades e Assistência Contextual

## 1. Visão Geral
Transforma a interface desktop do projeto (anteriormente focada em chat) em um companheiro local passivo e contextual. O agente observa as atividades em background (via sensores), processa observações no ciclo cognitivo e utiliza inferência local para oferecer assistência relevante e não intrusiva.

## 2. Contratos

### 2.1 CurrentActivity
Define a percepção atual do que o usuário está fazendo.
- `activity_id`: UUID
- `description`: String descritiva (ex: "Escrevendo código no VS Code")
- `application`: Identificador do app
- `started_at`: Timestamp
- `confidence`: double
- `related_memories`: Lista de UUIDs

### 2.2 ContextualAssistanceCard
Uma sugestão, observação ou pergunta proativa gerada pelo agente.
- `card_id`: UUID
- `title`: String
- `body`: String
- `action_label`: String
- `card_type`: "suggestion" | "observation" | "question"
- `relevance_score`: double

## 3. Requisitos de Produto

1. **Deny-by-default (SPEC-030)**: O aplicativo deve bloquear a inicialização de sensores e exigir consentimento explícito em seu primeiro uso (Onboarding).
2. **Ciclo Integrado**: Nenhuma ação da interface do usuário deve pular o ciclo cognitivo. Mensagens de texto do usuário entram no ciclo, que decide se responde textualmente ou realiza uma ação interna.
3. **Interface Companheira**: O modo primário da interface (compacto) não é um prompt, mas um display do status atual (Atividade + Cards de Assistência).
4. **Visibilidade de Sensores**: O usuário deve ter controle explícito para pausar ou revogar permissões de cada sensor.

## 4. Critérios de Aceite

- [x] O aplicativo exige aceitação no primeiro uso antes de ligar a câmera/capturar tela.
- [x] O loop UI -> Ollama foi quebrado e agora passa pelo `CognitiveCoordinator`.
- [x] O widget na bandeja mostra a "Atividade Atual".
- [x] O widget na bandeja mostra um "Card de Assistência" quando o ciclo cognitivo gera uma decisão relevante.
- [x] Os sensores podem ser ativados/desativados no menu de configurações.


## Objetivo
Cumprir a validacao.


## Escopo negativo
- Nada a declarar.


## Critérios de aceite
- [ ] Concluido.
