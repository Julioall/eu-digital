# ADR-0019 — Propriocepção digital e loop de agência

Status: aceito
Data: 2026-07-29

## Contexto

A SPEC-019 estava bloqueada por não possuir contratos para estado interno,
intenção, cópia eferente, resultado e atribuição. H7 propõe que uma previsão
de efeito vinculada a uma ação reversível melhore a distinção entre efeitos
próprios e mudanças externas.

## Decisão

Implementar primeiro uma referência local Python, com cinco contratos
versionados e um loop determinístico:

intenção → previsão/cópia eferente → resultado observado → erro preditivo →
atribuição auditável.

O tratamento usa a política agency_attribution_v1. O baseline
passive_observer_v0 usa a mesma interface, mas não registra cópia eferente.
Sem correlação suficiente, o resultado é ambiguous; ausência de correlação
não vira evidência de efeito externo. Somente uma marca explícita do
observador pode produzir external.

O módulo representa estado corporal funcional (capacidades, fila, ações,
falhas, limitações, latências e avatar), mas não afirma corpo biológico,
self fenomenal, intenção subjetiva ou consciência. A referência Python não é
ground truth nem é promovida ao runtime C++ nesta SPEC.

## Protocolo científico

- hipótese H7: previsão vinculada a ações reversíveis melhora F1 de atribuição
  própria/externa e reduz erro preditivo contra o baseline;
- métrica primária: macro-F1 de atribuição em fixtures com rótulo externo
  congelado; métricas secundárias: erro de efeito e adaptação após erro;
- ablação: passive_observer_v0 pela mesma interface, sem efference copy;
- falsificação: tratamento não supera o baseline no holdout, ou atribui
  external apenas por ausência de correlação;
- custo e proveniência são registrados junto ao resultado.

## Consequências

Positivas:

- a atribuição causal fica separada de observação e política;
- a ausência de evidência não é convertida em evento negativo;
- o mecanismo pode ser desligado e comparado por ablação;
- contratos permanecem disponíveis para futura promoção, sem antecipá-la.

Custos:

- fixtures precisam de ground truth de efeitos próprios e externos;
- correlação temporal imperfeita produz ambiguidade legítima;
- validade ecológica continua não demonstrada.

## Reversão

Desabilitar o loop retorna ao baseline passivo sem apagar eventos históricos.
Nenhum contrato público anterior é alterado.
