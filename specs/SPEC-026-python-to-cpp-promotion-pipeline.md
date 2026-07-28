# SPEC-026 — Pipeline de promoção Python para C++

Status: blocked
Fase: 0.5
Dependências: SPEC-018, SPEC-025
ADRs aplicáveis: ADR-0005, ADR-0008, ADR-0010
Contrato aplicável: CROSS_LANGUAGE_EQUIVALENCE_CONTRACT.md

## Objetivo

Implementar o processo reproduzível que transforma um mecanismo Python validado em componente C++ implantável.

## Entregáveis

- manifesto de promoção;
- congelamento de referência;
- gerador de fixtures;
- runner Python;
- runner C++;
- comparador de equivalência;
- relatório automático;
- gate de performance;
- registro de promoções aprovadas.

## Fluxo

1. selecionar hipótese validada;
2. congelar referência Python;
3. gerar dataset de equivalência;
4. definir tolerâncias;
5. implementar candidato C++;
6. executar comparação;
7. classificar divergências;
8. executar benchmark;
9. aprovar ou rejeitar;
10. registrar no manifesto do runtime.

## Critérios de aceite

- [ ] referência e candidato recebem bytes semanticamente idênticos.
- [ ] todas as divergências são persistidas.
- [ ] alteração de tolerância exige justificativa e nova revisão.
- [ ] CI bloqueia componente sem promoção aprovada.
- [ ] relatório associa hipótese, commits, dataset, hardware e métricas.
- [ ] runtime informa a promoção correspondente de cada componente cognitivo.

## Correção científica obrigatória

A referência Python não é ground truth. O pipeline deve comparar:

```text
Python ↔ contrato
C++ ↔ contrato
Python ↔ ground truth/invariantes
C++ ↔ ground truth/invariantes
Python ↔ C++
```

A promoção falha quando ambas as implementações concordam entre si, mas violam ground truth, holdout ou relação metamórfica.
