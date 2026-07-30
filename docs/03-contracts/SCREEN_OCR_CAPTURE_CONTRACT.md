# Contrato de captura visual e OCR consentidos

O schema executável é `contracts/schemas/screen_ocr_capture_policy.schema.json`.

Regras normativas:

- o consentimento é resolvido pelo ledger da SPEC-030; este contrato registra
  o estado resolvido, não concede permissão por si só;
- o default é `not_granted` + `disabled`;
- `explicit` exige consentimento concedido e não usa `request_id`;
- `on_demand` exige consentimento concedido e uma solicitação não vazia;
- `global_pause` tem precedência sobre os dois modos;
- região de interesse deve ser positiva e caber no frame antes do armazenamento;
- `redaction_version` é `length-only-v1`; texto bruto não entra no evento;
- `text_retention_days` é no máximo 7 dias e `visual_retention_days` no máximo
  30 dias;
- eventos usam `reference-and-hash-only`: caminho/hash e metadados, nunca bytes;
- `screen.ocr_unavailable` significa que a inferência OCR não estava
  disponível; não representa ausência de texto.

O sensor permanece independente das implementações concretas de captura e OCR.
`ImageStore` e `OcrEngine` são portas substituíveis e locais.
