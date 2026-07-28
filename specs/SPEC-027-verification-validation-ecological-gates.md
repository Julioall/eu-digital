---
id: SPEC-027
title: Gates de verificação, validade e transferência ecológica
status: done
phase: 0.5
dependencies: [SPEC-017, SPEC-018, SPEC-025, SPEC-026]
adrs: [ADR-0005, ADR-0008, ADR-0010, ADR-0011]
contracts: []
---

# SPEC-027 — Gates de verificação, validade e transferência ecológica

Status: done
Fase: 0.5
Dependências: SPEC-017, SPEC-018, SPEC-025, SPEC-026
ADRs aplicáveis: ADR-0005, ADR-0008, ADR-0010, ADR-0011

## Objetivo

Implementar gates independentes para verificação, equivalência computacional, validade científica e validade ecológica.

## Requisitos

1. ground-truth fixtures;
2. holdout bloqueado;
3. testes metamórficos;
4. relógio virtual;
5. injeção de falhas e jitter;
6. comparação entre Python e C++;
7. comparação entre backends e hardware;
8. auditoria de exportação e quantização;
9. sessões online controladas;
10. relatório longitudinal;
11. revisão independente ou protocolo congelado.

## Escopo negativo

- implementar mecanismos cognitivos, sensores ou funcionalidades de produto;
- definir consciência fenomenal como objeto mensurável;
- substituir avaliações específicas de cada domínio;
- usar equivalência Python–C++ como ground truth;
- apresentar métricas operacionais como evidência cognitiva;
- alterar contratos de promoção sem versionamento.

## Critérios de aceite

- [x] Equivalência com Python não é usada como único critério.
- [x] Pelo menos uma fixture possui verdade conhecida.
- [x] O holdout possui hash e acesso registrado.
- [x] Testes metamórficos detectam mutações deliberadas.
- [x] Replay controla relógio e ordem.
- [x] Falhas de sensor são reproduzíveis.
- [x] Modelo exportado possui relatório de diferença.
- [x] Teste online é executado após replay.
- [x] Relatório separa validade cognitiva de desempenho operacional.
