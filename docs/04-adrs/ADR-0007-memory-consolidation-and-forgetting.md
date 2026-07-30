# ADR-0007 — Consolidação, replay e esquecimento

Status: aceito

## Decisão

Memória não será um log infinito. O sistema terá processos explícitos de replay, consolidação, generalização, reconciliação, retenção e esquecimento.

## Consequência

Toda memória consolidada deve manter proveniência para episódios.
