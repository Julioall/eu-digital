# Contrato: Episode

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
