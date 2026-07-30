# ADR-0001 — Arquitetura modular orientada a eventos

Status: aceito

## Contexto

O sistema recebe sinais contínuos e heterogêneos, precisa ser extensível e deve operar com modelos pesados apenas sob demanda.

## Decisão

Usar arquitetura modular orientada a eventos, com contratos versionados e persistência append-only para eventos.

## Consequências

Positivas: desacoplamento, replay, auditoria, testes e extensibilidade.  
Negativas: maior complexidade operacional, necessidade de schemas e controle de ordenação.
