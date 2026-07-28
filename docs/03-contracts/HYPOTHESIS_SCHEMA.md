# Contrato: Hypothesis

A definição executável e versionada está em
`contracts/schemas/hypothesis.schema.json`. O YAML abaixo documenta a
semântica; Python e C++ devem consumir o schema compartilhado, sem manter
definições manuais divergentes.

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
