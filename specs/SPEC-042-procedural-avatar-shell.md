---
id: SPEC-042
title: Avatar procedural e shell local
status: future
phase: beta
dependencies: [SPEC-040, SPEC-041]
adrs: [ADR-0006, ADR-0009, ADR-0010, ADR-0011]
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

- [ ] Avatar renderiza sem modelo e sem assets de IA.
- [ ] Perfil visual é limitado, versionado e separado de diálogo/emoção.
- [ ] Fallback gráfico, pause global, quota e health são observáveis.
- [ ] Acessibilidade e critérios do spike passam.
- [ ] Nenhuma ação ou captura é executada pelo shell.

## Saída

Shell local com avatar procedural independente do núcleo cognitivo.

## Incremento headless permitido

O perfil sidecar e o renderer nativo CPU-first podem ser preparados sem um
host desktop. Essa camada expõe consentimento, pausa global, quota, health,
feedback e histórico local, mas não abre janela nem declara a SPEC concluída.
O shell Qt/QML e os critérios de acessibilidade continuam condicionados à
revisão humana da ADR-0032 e à matriz manual Windows.

O probe `procedural_avatar_probe` demonstra a saída de framebuffer local e
valida `avatar_frame.schema.json`; ele não é um shell de produto.
