# Contrato: CapabilityDescriptor

A definição executável deste contrato está em
`contracts/schemas/capability_descriptor.schema.json`.

```yaml
schema_version: "1.0"
capability_id: string
implementation_id: string
implementation_version: semver
kind: sensor|tool|actuator|model|cognitive_service

provides:
  - operation: string
    input_schema: uri|null
    output_schema: uri
    modalities: [string]
    side_effect: none|reversible|irreversible
    streaming: boolean

requires:
  mandatory:
    - operation: string
      version_range: string|null
  optional:
    - operation: string
      version_range: string|null

runtime:
  execution: in_process|subprocess|ipc
  startup: eager|lazy|manual
  supports_hot_plug: boolean
  supports_checkpoint: boolean
  estimated_ram_mb: integer|null
  estimated_cpu_class: low|medium|high|unknown

quality:
  confidence_model: string|null
  latency_class: realtime|interactive|batch
  calibration_required: boolean

permissions: [string]
config_schema: uri|null
health_check_operation: string
```

## Regras

- `capability_id` representa capacidade lógica; `implementation_id` representa plugin concreto.
- Mais de uma implementação pode fornecer a mesma operação.
- O núcleo seleciona por operação e restrições.
- Toda operação com efeito deve declarar reversibilidade.
- Manifests inválidos nunca entram no registro.
