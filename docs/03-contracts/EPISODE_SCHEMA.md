# Contrato: Episode

A definição executável e versionada deste contrato está em
`contracts/schemas/episode.schema.json`. A segmentação não preenche `people`,
`topics`, `summary` ou `embedding_ref` sem observação explícita; ausência fica
representada por listas vazias ou `null`.

```yaml
episode_id: uuid
schema_version: "1.0"
session_id: uuid
start_at: ISO-8601
end_at: ISO-8601
event_ids: [uuid]
context_summary:
  applications: [string]
  documents: [string]
  people: [string]
  topics: [string]
  modalities: [string]
boundary_reasons: [string]
embedding_ref: string|null
summary: string|null
hypotheses: [uuid]
quality:
  coherence: number
  confidence: number
created_by: string
```
