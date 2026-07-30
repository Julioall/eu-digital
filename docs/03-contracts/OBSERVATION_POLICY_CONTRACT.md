# Contrato de política de observação Windows

O schema executável é `contracts/schemas/capture_policy.schema.json`.

Regras:

- executável e categoria são permitidos por padrão quando o aplicativo não
  está bloqueado;
- título de janela e clipboard são `false` por padrão;
- conteúdo textual só pode ser habilitado com allowlist explícita;
- a denylist mandatória não pode ser removida por uma configuração local;
- `global_pause` prevalece sobre qualquer concessão;
- o redator `length-only-v1` publica no máximo o comprimento do título;
- a supressão ocorre antes de construir o evento canônico e deixa health
  observável.

Este contrato não implementa OCR nem captura de tela. A autorização e retenção
específicas do OCR estão definidas na SPEC-032 e em
`SCREEN_OCR_CAPTURE_CONTRACT.md`.
