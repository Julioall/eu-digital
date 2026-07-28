# Contrato: CapabilityState

A definição executável deste contrato está em
`contracts/schemas/capability_state.schema.json`.

```yaml
capability_id: string
implementation_id: string
state: unknown|discovered|calibrating|available|degraded|temporarily_unavailable|disabled|failed|removed|incompatible
changed_at: ISO-8601
reason_code: string|null
message: string|null

health:
  score: number|null
  last_check_at: ISO-8601|null
  consecutive_failures: integer

performance:
  latency_ms_p50: number|null
  latency_ms_p95: number|null
  success_rate: number|null
  confidence_calibration: number|null

availability:
  since: ISO-8601|null
  expected_recovery_at: ISO-8601|null

impact:
  affected_operations: [string]
  blocked_goal_ids: [uuid]
  confidence_adjustment: number|null
```
