# SPEC-020 — Consolidação e esquecimento

Status: blocked
Fase: 3
Dependências: SPEC-008, SPEC-018
ADRs: ADR-0007

## Objetivo
Consolidar episódios em conhecimento semântico, aplicar replay e controlar retenção.

## Requisitos
- fila de consolidação;
- replay;
- generalização com proveniência;
- reconciliação;
- decay e arquivamento;
- métricas de retenção.

## Escopo negativo
Apagar fonte de uma crença consolidada.

## Critérios de aceite
- [ ] Replay reduz esquecimento no corpus.
- [ ] Conhecimento consolidado aponta para episódios.
- [ ] Políticas de retenção são reversíveis em teste.
