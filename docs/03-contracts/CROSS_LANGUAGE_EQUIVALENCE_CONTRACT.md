# Contrato de Equivalência entre Python e C++

## 1. Registro de promoção

```yaml
promotion_id: string
component_id: string
component_version: semver

hypothesis:
  id: string
  report_uri: string

reference:
  language: python
  package: string
  commit: string
  entrypoint: string
  environment_lock_hash: string

candidate:
  language: cpp
  target: string
  commit: string
  compiler: string
  build_profile: string

contract:
  input_schema: uri
  output_schema: uri
  state_schema: uri|null
  error_schema: uri
  clock_semantics: string
  random_seed_policy: string

dataset:
  fixture_set: uri
  hash: string
  case_count: integer

equivalence:
  type: exact|numeric|statistical|behavioral
  absolute_tolerance: number|null
  relative_tolerance: number|null
  invariants: [string]
  acceptance_metrics: object

performance:
  baseline_hardware: string
  maximum_latency_ms: number|null
  maximum_memory_mb: number|null
  minimum_throughput: number|null

status: draft|reference_frozen|candidate_ready|validated|rejected|promoted
```

## 2. Regras

- A referência Python deve estar congelada antes da comparação final.
- O dataset final não pode ser ajustado após observar falhas da versão C++ sem abrir nova rodada.
- Diferenças devem ser classificadas como bug, tolerância esperada ou alteração de especificação.
- Alterações de especificação invalidam a promoção e exigem nova versão.
- Performance não compensa divergência semântica.
- Equivalência textual de respostas de LLM não é exigida; invariantes e métricas devem ser usados.
- Todo relatório deve incluir casos divergentes, mesmo quando a promoção é aprovada.
