---
id: SPEC-041
title: Spike de interface desktop Windows
status: done
phase: beta
dependencies: [SPEC-028, SPEC-040]
adrs: [ADR-0009, ADR-0010, ADR-0011, ADR-0032]
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

- [x] Matriz de testes Windows documenta cada cenário e resultado em
      `docs/06-operations/DESKTOP_INTERFACE_SPIKE_MATRIX.md` e no relatório
      JSON gerado pelo probe.
- [x] O probe é um alvo opcional isolado e não altera a porta
      `AvatarPresentationPort`.
- [x] O probe validou ausência de foco e clipboard; a limitação de fullscreen
      foi reproduzida e registrada como resultado negativo, sem iniciar shell
      de produto.
- [x] Como SDL2/ImGui não cobre acessibilidade, tray, click-through e
      lifecycle completo, a ADR substitutiva ADR-0032 foi aberta antes de
      qualquer implementação do shell.

## Saída

Decisão técnica de interface, não um shell de produto concluído.

## Evidência

O alvo opcional `desktop_interface_spike` exercita SDL2 2.32.10 e Dear ImGui
1.92.8 em Windows/MSVC. A matriz e o resultado negativo ficam registrados em
`docs/06-operations/DESKTOP_INTERFACE_SPIKE_MATRIX.md`,
`validation/reports/desktop_interface_spike_windows.json` e
`docs/04-adrs/ADR-0032-desktop-interface-substitution.md`.
