# ADR-0028 — OCR e captura visual consentidos como capacidade independente

Status: accepted  
Data: 2026-07-29  
Decisores: aprovação humana do projeto

## Contexto

Captura visual e OCR podem expor documentos, pessoas e credenciais. O sensor
existente possui portas locais substituíveis, mas precisava de uma autorização
explícita e de uma distinção observável entre captura visual e disponibilidade
do OCR.

## Opções consideradas

1. Manter captura sempre habilitada e confiar somente no usuário do processo.
2. Bloquear OCR, mas continuar persistindo frames completos.
3. Exigir consentimento resolvido, habilitação explícita ou solicitação
   pontual, limitar a região, redigir texto e manter referências/hash no
   payload.

## Decisão

Adotar a opção 3:

- consentimento é resolvido fora do sensor e negar é o default;
- `explicit` habilita uma sessão consentida; `on_demand` exige um identificador
  de solicitação por chamada;
- pausa global e revogação bloqueiam antes do `ImageStore`;
- uma região válida é recortada antes de ser persistida;
- payloads usam `reference-and-hash-only`;
- `length-only-v1` redige cada palavra OCR;
- texto tem retenção de 7 dias e carrega metadado de retenção;
- falha do OCR preserva o evento visual e emite estado próprio de
  indisponibilidade.

O `ImageStore` e o `OcrEngine` continuam injetados. Nenhum backend Windows,
modelo ou dependência externa é escolhido por este ADR.

## Consequências

- captura não autorizada falha de forma previsível e auditável;
- OCR não consegue introduzir texto bruto no evento;
- armazenar somente a região reduz exposição, mas exige validação de limites;
- consumidores devem distinguir `screen.visual_captured`, `screen.ocr` e
  `screen.ocr_unavailable`;
- a capacidade permanece removível e pode ficar degradada sem parar o runtime.

## Plano de reversão

Desabilitar a capacidade ou revogar o consentimento. O runtime permanece em
`degraded`, sem reescrever eventos históricos nem ativar fallback semântico.
