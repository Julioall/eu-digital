---
id: SPEC-006
title: Timeline persistente
status: done
phase: 2
dependencies: [SPEC-002]
adrs: []
contracts: []
---

# SPEC-006 — Timeline persistente

Status: done
Fase: 2
Dependências: SPEC-002

## Objetivo
Persistir e consultar eventos por tempo, sessão, fonte, aplicativo e correlação.

## Requisitos
- SQLite inicial.
- Append-only.
- Índices temporais.
- Paginação.
- Migrações.
- Exportação e replay.

## Escopo negativo
Memória semântica e embeddings.

## Critérios de aceite
- [x] Reinicialização preserva eventos.
- [x] Consulta temporal retorna ordem correta.
- [x] Migração é testada.
- [x] Replay reproduz sequência determinística.
