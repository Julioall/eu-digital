# Contrato de adaptação de capacidades

Os contratos executáveis da SPEC-024 são:

- `capability_adaptation_event.schema.json`: mudança observada, impacto em
  crenças/predições/planos e ajustes auditáveis;
- `observability_profile.schema.json`: modalidades disponíveis, cegueiras,
  atenção redistribuída, confiança e histórico;
- `capability_onboarding.schema.json`: calibração progressiva de nova
  modalidade antes de influência estável.

Ausência, falha ou remoção altera a observabilidade e a confiança operacional;
nunca cria um evento negativo nem apaga memória. `agent_id` e
`identity_generation` permanecem constantes na substituição ou retorno de uma
capacidade.
