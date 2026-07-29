# Contrato de avaliação longitudinal

Os contratos executáveis da SPEC-022 são:

- `longitudinal_protocol.schema.json`: protocolo, critérios, hash do holdout e
  hash do protocolo congelado;
- `longitudinal_snapshot.schema.json`: checkpoint de 7, 30 ou 90 dias com
  métricas cognitivas/operacionais separadas e versão do self-model;
- `longitudinal_report.schema.json`: curva de retenção, baseline cronológico,
  ganhos/perdas e drift do self-model.

O relatório é derivado apenas dos snapshots vinculados ao mesmo protocolo. A
ausência de uma métrica não é convertida em zero nem em perda. `retention_score`
e `calibration_ece` são nomes convencionais para a curva e calibração quando
fornecidos pelo estudo; outras métricas continuam versionadas no snapshot.
