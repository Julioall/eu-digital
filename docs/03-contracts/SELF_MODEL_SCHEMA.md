# Contrato: SelfModel

```yaml
self_model_version: integer
identity:
  name: string
  role: string
  owner_user_id: string
capabilities:
  available: [string]
  degraded: [string]
  temporarily_unavailable: [string]
  disabled: [string]
  removed: [string]
  potential: [string]
  capability_history: [string]
observability:
  available_modalities: [string]
  unavailable_modalities: [string]
  known_blind_spots: [string]
sensors:
  active: [string]
  degraded: [string]
state:
  mode: observing|interpreting|asking|suggesting|acting|degraded
  active_goal_ids: [uuid]
  workspace_item_ids: [uuid]
knowledge:
  learned_pattern_count: integer
  unresolved_contradictions: integer
confidence:
  global: number
  calibration_score: number|null
continuity:
  created_at: ISO-8601
  last_updated_at: ISO-8601
  prior_version_hash: string|null
```
