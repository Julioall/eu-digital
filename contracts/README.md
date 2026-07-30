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

Os contratos executáveis de metacognição e curiosidade da SPEC-011 estão em
`hypothesis.schema.json`, `metacognitive_assessment.schema.json`,
`curiosity_question.schema.json` e `curiosity_response.schema.json`. A
semântica está documentada em
`docs/03-contracts/METACOGNITION_CURIOSITY_SCHEMA.md`.

Os contratos complementares de modelo de si funcional da SPEC-012 estão em
`self_model_internal_event.schema.json`,
`functional_self_model_snapshot.schema.json` e `self_model_decision.schema.json`.
Eles preservam `self_model.schema.json` da SPEC-023; a semântica está em
`docs/03-contracts/FUNCTIONAL_SELF_MODEL_SCHEMA.md`.

Os contratos do gateway de modelo local da SPEC-013 estão em
`model_prompt_template.schema.json`, `local_model_request.schema.json` e
`local_model_response.schema.json`. A semântica está documentada em
`docs/03-contracts/LOCAL_MODEL_GATEWAY_SCHEMA.md`.

Os contratos locais da SPEC-014 estão em `dialogue_notice.schema.json`,
`dialogue_feedback.schema.json` e `avatar_view_state.schema.json`. A semântica
está documentada em `docs/03-contracts/DIALOGUE_AVATAR_SCHEMA.md`.

O perfil procedural sidecar da SPEC-042 está em
`avatar_presentation_profile.schema.json`. Ele não altera o schema 1.0 de
`AvatarViewState`; a implementação nativa headless permanece separada do host
desktop e da escolha de toolkit.

O output observável do renderer headless está em `avatar_frame.schema.json`.
O probe CLI só emite metadados locais e digest do framebuffer; não captura
input, exige modelo ou abre uma janela.

Os contratos locais da SPEC-015 estão em `audio_segment.schema.json` e
`audio_transcription.schema.json`. A semântica está documentada em
`docs/03-contracts/AUDIO_SENSOR_SCHEMA.md`.

Os contratos locais da SPEC-016 estão em action_plan.schema.json,
action_simulation.schema.json, action_authorization.schema.json e
action_outcome.schema.json. A semântica está documentada em
docs/03-contracts/SUPERVISED_ACTION_SCHEMA.md.

Os contratos locais da SPEC-019 estão em digital_body_state.schema.json,
action_intention.schema.json, efference_copy.schema.json,
agency_action_outcome.schema.json e agency_attribution.schema.json. A
semântica está documentada em
docs/03-contracts/DIGITAL_PROPRIOCEPTION_AGENCY_SCHEMA.md.

Os contratos locais da SPEC-020 estão em semantic_knowledge.schema.json,
consolidation_record.schema.json e retention_decision.schema.json. A
semântica está documentada em docs/03-contracts/MEMORY_CONSOLIDATION_SCHEMA.md.

Os contratos locais da SPEC-021 estão em world_model_prediction.schema.json,
prediction_error.schema.json e prediction_drift.schema.json. A semântica está
documentada em docs/03-contracts/WORLD_MODEL_PREDICTION_SCHEMA.md.

Os contratos locais da SPEC-022 estão em longitudinal_protocol.schema.json,
longitudinal_snapshot.schema.json e longitudinal_report.schema.json. A
semântica está documentada em docs/03-contracts/LONGITUDINAL_EVALUATION_SCHEMA.md.

Os contratos locais da SPEC-024 estão em capability_adaptation_event.schema.json,
observability_profile.schema.json e capability_onboarding.schema.json. A
semântica está documentada em docs/03-contracts/CAPABILITY_ADAPTATION_SCHEMA.md.

Os schemas executáveis de capacidades da SPEC-023 são:

- `capability_descriptor.schema.json`;
- `capability_state.schema.json`;
- `observation_envelope.schema.json`;
- `self_model.schema.json`;
- `plugin_manifest.schema.json`.

Os contratos de privacidade e armazenamento da SPEC-030 são:

- `consent_policy.schema.json`;
- `storage_policy.schema.json`;
- `storage_health.schema.json`;
- `data_management_request.schema.json`.

A semântica está documentada em
`docs/03-contracts/PRIVACY_STORAGE_CONTRACTS.md`. Defaults de retenção e quota
são versionados e não autorizam exclusão automática.

O contrato de política de captura de baixo risco da SPEC-031 é
`capture_policy.schema.json`, documentado em
`docs/03-contracts/OBSERVATION_POLICY_CONTRACT.md`.

O contrato de captura visual e OCR consentidos da SPEC-032 é
`screen_ocr_capture_policy.schema.json`, documentado em
`docs/03-contracts/SCREEN_OCR_CAPTURE_CONTRACT.md`.

Python e C++ consomem esses contratos; nenhum plugin concreto faz parte do
núcleo.

O contrato de equivalência usado pela SPEC-026 está documentado em
`docs/03-contracts/CROSS_LANGUAGE_EQUIVALENCE_CONTRACT.md`. O pipeline grava
manifestos e relatórios em `promotions/` e mantém divergências mesmo quando a
promoção é aprovada.
