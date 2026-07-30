# Contrato de maturidade de componentes

O registro `component_maturity.json` separa três estados que não podem ser
inferidos uns dos outros:

- `reference_status`: maturidade da implementação de referência (`none`,
  `python`, `frozen`);
- `native_status`: situação da implementação C++ (`none`, `candidate`,
  `equivalent`, `promoted`);
- `product_status`: disponibilidade no produto (`unavailable`,
  `experimental`, `beta`, `released`).

O `status` da SPEC continua sendo documental e permanece no arquivo da SPEC.
O registro não o substitui nem transforma `done` em disponibilidade de produto.

Regras mínimas:

- `equivalent` e `promoted` exigem referência congelada;
- `promoted` exige `promotion_id`;
- `beta` e `released` exigem implementação nativa promovida;
- cada `component_id` deve ser único, cada referência de SPEC deve ser
  resolvível e cada `evidence_ref` deve apontar para um arquivo dentro do
  repositório;
- o registro não aprova promoção: ele apenas representa o estado já
  evidenciado pelo pipeline da SPEC-026.
