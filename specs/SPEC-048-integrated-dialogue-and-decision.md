---
id: SPEC-048
title: Integrated Dialogue and Decision Output
status: draft
created: 2026-07-31
authors: [Julio]
---

# SPEC-048: Integrated Dialogue and Decision Output

## 1. Motivação
Temos um `SuggestionOrchestrator` implementado (SPEC-043) e o `LocalModelGateway` capaz de gerar diálogos (SPEC-040). Porém, as decisões de quando invocar o modelo local para falar com o usuário, baseadas no estado da área de trabalho global (`GlobalWorkspace`), precisam de uma ponte algorítmica clara. O sistema precisa converter um estado cognitivo (memórias + surpresa + restrições do self) em uma das 5 saídas: silêncio, pergunta, resposta, sugestão ou pedido de confirmação.

## 2. Objetivo
Criar o módulo integrador `DecisionOutputRouter` (como estágio final do ciclo cognitivo) que recebe o pacote do `GlobalWorkspace` + `Metacognition`, consulta o `SuggestionOrchestrator` sobre o "orçamento" de interrupção, e se aprovado, usa o `LocalModelGateway` (Prompting rigoroso) para gerar a fala final, invocando a interface gráfica (Avatar).

## 3. Escopo Positivo
- Criar a ponte algorítmica entre o estado cognitivo vetorial e a geração de prompt para o LLM local (System Prompt Dinâmico baseado no self-model).
- Passar o output final para o `ProceduralAvatarShell` (Qt).
- Respeitar 100% das negativas do `SuggestionOrchestrator` (Silêncio forçado).

## 4. Escopo Negativo
- Não treinar/fine-tunar o LLM local. Apenas engenharia de prompt restrita.
- Não manter histórico conversacional ad-hoc (a memória de longo prazo já faz isso).

## 5. Aceite Mensurável
- Quando o orçamento de sugestões diário acabar, o sistema emite garantidamente um evento de silêncio (ação = none).
- O prompt enviado ao modelo local deve conter o snapshot das limitações do self-model para evitar alucinações de agência.
