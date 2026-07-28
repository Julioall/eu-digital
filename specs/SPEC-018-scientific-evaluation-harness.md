---
id: SPEC-018
title: Harness de avaliação, ablação e validade
status: blocked
phase: 0.5
dependencies: [SPEC-017, SPEC-025]
adrs: [ADR-0005, ADR-0008, ADR-0011]
contracts: []
---

# SPEC-018 — Harness de avaliação, ablação e validade

Status: blocked
Fase: 0.5
Dependências: SPEC-017, SPEC-025
ADRs aplicáveis: ADR-0005, ADR-0008, ADR-0011

## Objetivo

Executar baselines, ablações, métricas, verificação e relatórios reproduzíveis.

## Requisitos

- configuração por experimento;
- feature flags por módulo;
- seeds e fontes de nondeterminismo;
- coleta de recursos;
- métricas cognitivas e operacionais separadas;
- comparação estatística;
- ground-truth fixtures;
- datasets de desenvolvimento, validação e holdout;
- testes metamórficos;
- relógio virtual e replay;
- injeção de falhas;
- relatório automático;
- registro de divergências;
- intervalos de incerteza.

## Escopo negativo

- escolher vencedor sem critérios pré-definidos;
- usar equivalência de software como única evidência;
- ajustar o holdout após observação;
- tratar latência como métrica cognitiva.

## Critérios de aceite

- [ ] Módulo pode ser desativado sem alteração de código.
- [ ] Experimento registra commit, hardware, backend e configuração.
- [ ] Relatório compara baseline e tratamento.
- [ ] Pelo menos uma condição utiliza ground truth.
- [ ] Testes metamórficos detectam mutação conhecida.
- [ ] O holdout possui hash e política de acesso.
- [ ] Resultados cognitivos e operacionais são apresentados separadamente.
