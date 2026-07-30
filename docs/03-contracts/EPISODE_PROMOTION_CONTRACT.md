# Contrato de promoção: segmentação de episódios

Este contrato define o envelope de avaliação da implementação nativa. O
contrato de produto continua sendo `contracts/schemas/episode.schema.json`.

Cada linha de fixture contém `case_id`, `config` e uma lista ordenada de
eventos. `ground_truth` e `invariants` são metadados de avaliação e não são
consumidos pelo candidato C++.

```json
{
  "case_id": "episode-example",
  "config": {
    "max_gap_seconds": 3.0,
    "split_on_application_change": true,
    "split_on_document_change": true
  },
  "events": [],
  "ground_truth": [{"event_ids": ["event-1"]}],
  "invariants": ["all_events_covered"]
}
```

O runner nativo recebe as linhas por stdin e emite uma linha por caso. O
resultado deve conter `schema_version`, `baseline_id`, `config_fingerprint`,
`boundaries` e episódios completos, incluindo `context_summary`, qualidade,
`created_by` e razões de fronteira. A entrada é transmitida sem transformação
ao runner de referência e ao candidato.

O conjunto em `validation/holdout/` é separado do desenvolvimento e sua hash
é registrada antes da avaliação. Nenhuma alteração no holdout pode ser feita
para corrigir um resultado sem abrir uma nova versão de promoção.

O manifesto também registra `reference.source_path` e
`reference.source_sha256` para congelar o arquivo Python do entrypoint antes da
comparação final. Essa hash identifica a referência computacional; não é um
oráculo de validade científica.
