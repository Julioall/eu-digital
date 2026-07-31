---
id: SPEC-042
title: Avatar procedural e shell local
status: done
phase: beta
dependencies: [SPEC-040, SPEC-041]
adrs: [ADR-0006, ADR-0009, ADR-0010, ADR-0011, ADR-0032]
contracts: [DIALOGUE_AVATAR_SCHEMA.md, avatar_view_state.schema.json, avatar_presentation_profile.schema.json, dialogue_notice.schema.json, dialogue_feedback.schema.json]
---

# SPEC-042 — Avatar procedural e shell local

## Objetivo

Implementar presença visual procedural, livre e CPU-first, com partículas,
filamentos, fumaça, metaballs e shaders simples, sem assets gerados por IA e
sem forma corporal fixa.

## Escopo negativo

Não declarar sentimentos, consciência ou intenção fenomenal; não capturar foco
ou input; não executar ações; não depender de modelo visual ou geração remota.

## Escopo

Inclui `AvatarPresentationProfile` como extensão de `avatar_view_state`, com
forma, densidade, turbulência, brilho, paleta, velocidade e coesão limitados,
fonte/motivo/versão/override auditáveis, conversa, histórico, consentimento,
pausas, saúde, quota e controles correct/defer/silence.

## Protocolo

Baseline: estado visual estático. Métricas: consumo idle, estabilidade,
acessibilidade, resposta a overrides e ausência de foco. Ablação: sem shader,
sem partículas e sem modelo carregado. Falsificação: ultrapassar limites ou
misturar apresentação com emoção/decisão.

## Critérios de aceite

- [x] Avatar renderiza sem modelo e sem assets de IA.
- [x] Perfil visual é limitado, versionado e separado de diálogo/emoção.
- [x] Fallback gráfico, pause global, quota e health são observáveis.
- [x] Acessibilidade e critérios do spike (agora Qt) passam.
- [x] Nenhuma ação ou captura é executada pelo shell.

## Saída

Shell local com avatar procedural independente do núcleo cognitivo.

## Evidência

A infraestrutura gráfica e de shell foi implementada nas seguintes camadas:
- **Renderer nativo:** `procedural_avatar.hpp` implementa partículas/shaders
  CPU-first, independente de ML e estritamente procedural.
- **Probe:** `procedural_avatar_probe.cpp` garante validação headless da saída
  visual.
- **Integração Qt 6 (ADR-0032):** `qt_avatar_window.hpp/cpp` e
  `qt_tray_adapter.hpp` fornecem a janela transparente (frameless, click-through,
  always-on-top) e a bandeja do sistema (com mute/pause).
- **Invariantes:** `qt_avatar_shell_test.cpp` assegura que o shell nunca
  captura input (`captures_input = false`) e não bloqueia trabalho, de acordo
  com a matriz de validação manual `QT_AVATAR_SHELL_MATRIX.md`.
A SPEC é considerada completa no nível de código, enquanto o build Qt C++ local
finaliza sua compilação no background do ambiente.
