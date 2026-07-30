# ADR-0025 — Separação entre maturidade documental, nativa e de produto

Status: accepted
Data: 2026-07-29
Decisores: aprovação humana do projeto

## Contexto

As SPECs concluídas registram contratos, referências Python, testes ou
implementações experimentais. Um único `done` não informa se existe uma
implementação C++ equivalente ou uma capacidade distribuída no produto.

## Decisão

Manter o status documental da SPEC em seu próprio arquivo e registrar por
componente, em contrato versionado separado, `reference_status`,
`native_status` e `product_status`. O registro pode apontar para evidências e
para um `promotion_id`, mas não pode criar uma promoção por declaração.

Somente uma implementação nativa promovida pode receber estado de produto
`beta` ou `released`. A ausência de promoção mantém a capacidade
`unavailable`, mesmo que a SPEC ou a referência Python esteja `done`.

## Consequências

- releases podem ser auditadas sem reinterpretar o histórico das SPECs;
- a promoção C++ continua sob os gates das SPECs 026 e 027;
- o registro é uma fonte de estado de release, não uma segunda implementação
  cognitiva;
- migrações devem versionar o schema antes de adicionar novos estados.
