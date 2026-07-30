# Contrato de world model e erro preditivo

Os contratos executáveis desta SPEC são:

- `contracts/schemas/world_model_prediction.schema.json`: distribuição de
  próximos estados, contexto, resultado opcional e contribuição auditável para
  saliência;
- `contracts/schemas/prediction_error.schema.json`: erro observado, log loss,
  top-k e vínculo com drift;
- `contracts/schemas/prediction_drift.schema.json`: janela que excedeu o
  limiar, redução de confiança e início de reaprendizagem.

## Políticas

`frequency_baseline_v0` usa somente a frequência marginal observada.
`markov_order1_v0` usa a transição do último estado. O tratamento
`incremental_markov_v1` usa n-gramas incrementais com fallback para contextos
menores e depois para a distribuição global.

Uma previsão é criada antes do estado seguinte ser observado. Após a
observação, o mesmo `prediction_id` recebe log loss e `top_k_hit`. O valor
`salience_contribution` é um fator observado para o workspace; não é uma
decisão de seleção por si só.

Drift não apaga observações. Ele limpa apenas contagens derivadas do modelo,
reduz a confiança e marca que as observações seguintes iniciam reaprendizagem.

## Promoção nativa SPEC-036

O candidato C++ recebe os mesmos JSON-lines da referência Python pelo runner
`promotion_fixture_runner --world-model`. Cada caso pode intercalar
`observe`, `predict` e `score`; uma previsão sem resultado observado mantém
`observed_state`, `log_loss` e `top_k_hit` nulos. O conjunto de desenvolvimento
e o holdout são congelados em
`validation/equivalence/world_model_prediction_v1.jsonl` e
`validation/holdout/world_model_prediction_v1_holdout.jsonl`, com hashes no
manifesto `promotions/cognition.world_model.v1.json`.

O campo opcional `patterns` recebe referências de padrões com
`pattern_id`, `status: promoted` e confiança validada. Esses IDs formam apenas
um vocabulário simbólico de estados possíveis; a confiança não é convertida em
fato, nome, intenção ou ação. Padrões candidatos, duplicados ou inválidos são
rejeitados, e a ausência de padrões não é interpretada como evidência negativa.

Equivalência computacional, calibração, baseline, ablação e benchmark são
evidência operacional da promoção, não ground truth nem evidência cognitiva.
O componente permanece indisponível no produto sem revisão humana registrada.
