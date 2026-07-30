# Contrato de promoção: aprendizagem incremental de padrões

O contrato de produto dos padrões é `contracts/schemas/pattern.schema.json`.
Este envelope define a avaliação cross-language e não autoriza nomeação,
execução ou ação.

Cada linha contém `case_id`, `stream_id`, `config` e `operations`. Uma operação
`observe` contém features numéricas, referência e timestamp. Uma operação
`feedback` contém `target`, `positive` e referência humana; o alvo
`last_observation` resolve para o padrão retornado pela observação anterior.

O resultado contém observações, feedback, snapshot e métricas. Todo padrão
preserva `observation_refs`, feedback, versão, status, centroid, parent e
`drift_reason`. O estado `promoted` significa somente que suporte e confiança
atingiram os limites configurados; não é uma verdade sobre o mundo.

`ground_truth` e `invariants` nas fixtures são metadados de avaliação. O
holdout em `validation/holdout/` é congelado antes do runner ser comparado.
