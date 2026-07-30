---
id: SPEC-041
title: Spike de interface desktop Windows
status: future
phase: beta
dependencies: [SPEC-028, SPEC-040]
adrs: [ADR-0009, ADR-0010, ADR-0011]
contracts: [DIALOGUE_AVATAR_SCHEMA.md, avatar_view_state.schema.json]
---

# SPEC-041 — Spike de interface desktop Windows

## Objetivo

Validar tecnicamente SDL2 + Dear ImGui, ou produzir ADR substitutiva, antes de
fixar o renderer e o shell do produto.

## Escopo negativo

Não integrar captura sensorial, modelo obrigatório, ações, avatar final ou
autonomia. O spike não pode alterar `AvatarPresentationPort`.

## Escopo

Inclui IME/pt-BR, DPI 100–250%, múltiplos monitores, teclado, leitor de tela,
transparência, click-through, tray, fullscreen, suspensão/retomada, foco e
consumo ocioso.

## Protocolo

Baseline: janela nativa mínima. Métricas: acessibilidade, consumo GPU/CPU,
foco, latência de input, DPI e recuperação. Ablação: SDL2 sem ImGui e ImGui
sem transparência. Falsificação: falha de acessibilidade ou comportamento
Windows essencial.

## Critérios de aceite

- [ ] Matriz de testes Windows documenta cada cenário e resultado.
- [ ] Renderer continua substituível atrás de `AvatarPresentationPort`.
- [ ] Não rouba foco, não captura clipboard e não bloqueia fullscreen.
- [ ] Se falhar, ADR substitutiva é aberta antes da implementação do shell.

## Saída

Decisão técnica de interface, não um shell de produto concluído.
