# Contrato: ObservationEnvelope

O envelope impede que o núcleo confunda ausência, falha e observação negativa.

```yaml
observation_id: uuid
capability_id: string
operation: string
occurred_at: ISO-8601

status: observed|no_signal|not_observable|sensor_degraded|sensor_failed

content_schema: uri
content: object|null

quality:
  confidence: number|null
  completeness: number|null
  resolution: string|null

observability:
  expected: boolean
  blind_spots: [string]
  missing_dependencies: [string]

provenance:
  implementation_id: string
  implementation_version: string
  calibration_id: string|null
```

## Regra crítica

`not_observable` nunca pode ser convertido em “não aconteceu”. Ausência de evidência deve permanecer distinta de evidência de ausência.
