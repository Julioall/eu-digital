---
id: SPEC-005
title: Tela e OCR
status: done
phase: 1
dependencies: [SPEC-002, SPEC-023]
adrs: []
contracts: []
---

# SPEC-005 — Tela e OCR

Status: done
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
- [x] Telas quase idênticas não geram OCR redundante.
- [x] Texto e coordenadas são persistidos.
- [x] Falha de OCR preserva o evento visual.
