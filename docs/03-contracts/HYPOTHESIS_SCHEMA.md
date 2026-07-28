# Contrato: Hypothesis

```yaml
hypothesis_id: uuid
kind: enum
statement: string
status: proposed|confirmed|rejected|superseded
confidence: number
evidence:
  supporting_refs: [string]
  opposing_refs: [string]
alternatives: [string]
created_at: ISO-8601
updated_at: ISO-8601
verification:
  question: string|null
  expected_information_gain: number|null
provenance:
  module: string
  model_version: string|null
```
