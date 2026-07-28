# SPEC-027 — Gates de verificação, validade e transferência ecológica

Status: blocked
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

## Critérios de aceite

- [ ] Equivalência com Python não é usada como único critério.
- [ ] Pelo menos uma fixture possui verdade conhecida.
- [ ] O holdout possui hash e acesso registrado.
- [ ] Testes metamórficos detectam mutações deliberadas.
- [ ] Replay controla relógio e ordem.
- [ ] Falhas de sensor são reproduzíveis.
- [ ] Modelo exportado possui relatório de diferença.
- [ ] Teste online é executado após replay.
- [ ] Relatório separa validade cognitiva de desempenho operacional.
