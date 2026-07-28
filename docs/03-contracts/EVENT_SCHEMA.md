# Contrato: CanonicalEvent

```yaml
schema_version: "1.0"
event_id: uuid
source: enum
event_type: string
occurred_at: ISO-8601
monotonic_ns: integer
received_at: ISO-8601
session_id: uuid
actor_id: string|null
context:
  process_name: string|null
  window_title: string|null
  document_uri: string|null
payload: object
quality:
  confidence: number
  completeness: number
  latency_ms: integer
provenance:
  sensor_id: string
  raw_event_id: string|null
privacy_class: enum
tags: [string]
```

## Regras

- Eventos são imutáveis.
- Correções geram novos eventos relacionados.
- Timestamp monotônico define ordem local.
- `payload` deve ser validado por subtipo.
- Nenhum módulo pode depender de texto livre como único identificador semântico.
