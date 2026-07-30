# Contratos de privacidade e armazenamento

Os schemas executáveis desta fase são:

- `consent_policy.schema.json` — decisões versionadas por sensor e finalidade,
  com negação padrão e pausa global;
- `storage_policy.schema.json` — retenção, quota, buckets contabilizados e
  comportamento de overflow;
- `storage_health.schema.json` — estado observável de quota e suspensão;
- `data_management_request.schema.json` — exportação ou exclusão explicitamente
  confirmada.

## Regras normativas

- ausência de uma concessão é `consent_not_granted`, nunca consentimento;
- revogação bloqueia novas capturas para o par sensor/finalidade;
- pausa global tem precedência sobre qualquer concessão;
- `database`, `wal`, `indexes`, `quarantine`, `backups` e `payloads` somam a
  quota do usuário;
- modelos são contabilizados separadamente;
- quota excedida exige `degraded`, suspensão e decisão do usuário;
- exportação e exclusão não podem ser executadas sem confirmação explícita;
- a proteção do armazenamento de consentimento usa Windows DPAPI; ausência da
  API é reportada, não substituída silenciosamente por texto puro.

Os contratos não concedem consentimento nem autorizam sensores concretos. A
integração dos adaptadores será tratada pela SPEC-031 e pela SPEC-032. A
política específica de captura Windows está em
`docs/03-contracts/OBSERVATION_POLICY_CONTRACT.md`.
