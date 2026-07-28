# Contratos compartilhados

Fonte normativa para schemas, protocolos, fixtures e compatibilidade entre Python e C++.

Definições semânticas não devem ser copiadas e mantidas manualmente em cada linguagem.

O schema executável compartilhado de `CanonicalEvent` está em
`contracts/schemas/canonical_event.schema.json`; fixtures de replay ficam em
`contracts/fixtures/`.

O schema executável de `Episode` da SPEC-007 está em
`contracts/schemas/episode.schema.json`.

O schema executável de padrões incrementais da SPEC-009 está em
`contracts/schemas/pattern.schema.json`.

Os contratos executáveis do workspace global da SPEC-010 estão em
`workspace_candidate.schema.json`, `workspace_item.schema.json`,
`workspace_snapshot.schema.json` e `workspace_broadcast.schema.json`. A
semântica está documentada em `docs/03-contracts/GLOBAL_WORKSPACE_SCHEMA.md`.

Os schemas executáveis de capacidades da SPEC-023 são:

- `capability_descriptor.schema.json`;
- `capability_state.schema.json`;
- `observation_envelope.schema.json`;
- `self_model.schema.json`;
- `plugin_manifest.schema.json`.

Python e C++ consomem esses contratos; nenhum plugin concreto faz parte do
núcleo.

O contrato de equivalência usado pela SPEC-026 está documentado em
`docs/03-contracts/CROSS_LANGUAGE_EQUIVALENCE_CONTRACT.md`. O pipeline grava
manifestos e relatórios em `promotions/` e mantém divergências mesmo quando a
promoção é aprovada.
