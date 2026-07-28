---
id: SPEC-005
title: Tela e OCR
status: blocked
phase: 1
dependencies: [SPEC-002, SPEC-023]
adrs: []
contracts: []
---

# SPEC-005 — Tela e OCR

Status: blocked
Fase: 1
Dependências: SPEC-002, SPEC-023

## Objetivo
Capturar imagens sob política adaptativa e extrair texto localmente.

## Requisitos
- Captura por mudança relevante ou intervalo.
- Hash perceptual para evitar duplicação.
- OCR local.
- Região de interesse.
- Referência ao arquivo de imagem, sem duplicar bytes no evento.

## Escopo negativo
Interpretação semântica profunda e vídeo contínuo.

## Critérios de aceite
- [ ] Telas quase idênticas não geram OCR redundante.
- [ ] Texto e coordenadas são persistidos.
- [ ] Falha de OCR preserva o evento visual.
