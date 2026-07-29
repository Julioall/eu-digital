# Contratos: Diálogo e Avatar

Os contratos da SPEC-014 são locais e representam apresentação, não
personalidade:

- `dialogue_notice.schema.json`: pergunta/notificação com hipótese, confiança,
  contexto e motivo;
- `dialogue_feedback.schema.json`: correção, adiamento ou silêncio do usuário;
- `avatar_view_state.schema.json`: estado visual com invariantes de não foco,
  não captura de input e não bloqueio do trabalho.

O host desktop é uma porta opcional. A ausência dele não apaga histórico nem
é tratada como observação negativa.
