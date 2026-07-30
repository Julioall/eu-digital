---
id: SPEC-032
title: OCR consentido como capacidade independente
status: done
phase: beta
dependencies: [SPEC-005, SPEC-030, SPEC-031]
adrs: [ADR-0002, ADR-0009, ADR-0026, ADR-0028]
contracts: [screen_ocr_capture_policy.schema.json]
---

# SPEC-032 — OCR consentido como capacidade independente

Status: done
Fase: Product Beta
Dependências: SPEC-005, SPEC-030, SPEC-031
ADRs aplicáveis: ADR-0002, ADR-0009, ADR-0026, ADR-0028

## Objetivo

Tornar OCR e captura visual capacidades locais, removíveis e explicitamente
autorizadas. A capacidade deve processar somente uma captura consentida, por
habilitação explícita ou por solicitação pontual, sem confundir indisponibilidade
do OCR com ausência de texto na tela.

## Entregáveis

- contrato versionado `ScreenOcrCapturePolicy`;
- referência Python para validação da política e do redator;
- sensor C++ com consentimento resolvido, habilitação explícita e solicitação
  pontual;
- validação de região e persistência somente da região selecionada;
- payloads com referência/hash, sem bytes de imagem nem texto OCR bruto;
- retenção textual reduzida e observável;
- health independente para autorização, captura visual e motor OCR;
- testes de ausência, revogação, pausa, falha, remoção e substituição das
  portas `ImageStore` e `OcrEngine`.

## Requisitos

1. O padrão é negar: sem consentimento resolvido, nenhuma captura visual ou
   OCR é aceita.
2. Uma captura só é aceita quando a política está em modo `explicit` ou
   quando `observe` recebe uma solicitação pontual não vazia; pausa global e
   revogação sempre prevalecem.
3. Região de interesse deve ser validada contra o frame e somente a região
   selecionada pode ser enviada ao `ImageStore`.
4. Eventos persistem somente caminho/referência, hash, dimensões e metadados;
   pixels não podem aparecer no payload.
5. O redator `length-only-v1` substitui cada texto OCR por marcador de
   comprimento; texto bruto não pode aparecer no evento.
6. Eventos textuais carregam retenção reduzida de 7 dias, distinta do default
   bruto de 30 dias, e o modo de payload é explicitamente versionado.
7. Falha ou ausência do motor OCR deve produzir health/estado `unavailable`
   próprio, preservando o evento visual e sem emitir uma observação negativa
   de que não há texto.
8. A capacidade publica `CapabilityDescriptor`, pode ser pausada, removida ou
   substituída sem alteração do núcleo cognitivo.

## Escopo negativo

- não capturar tela continuamente sem consentimento;
- não integrar captura Windows real nesta SPEC;
- não persistir bytes de imagem em eventos;
- não persistir texto OCR bruto;
- não interpretar semanticamente o texto;
- não usar modelo remoto, API externa ou telemetria;
- não integrar avatar, diálogo, modelo local ou ações;
- não promover mecanismo cognitivo.

## Critérios de aceite

- [x] Fixtures válida, default e inválida são validadas por schema e
      referência Python.
- [x] Consentimento ausente/revogado, pausa global e ausência de solicitação
      bloqueiam captura sem criar evento.
- [x] Habilitação explícita e solicitação pontual autorizam somente capturas
      válidas e registram a autorização.
- [x] Região inválida é rejeitada antes do armazenamento; região válida é a
      única enviada ao `ImageStore`.
- [x] Eventos não contêm pixels nem texto OCR bruto e usam o redator
      versionado.
- [x] Retenção textual reduzida e modo de payload aparecem nos eventos.
- [x] Falha do OCR preserva o visual e expõe `screen.ocr_unavailable`, sem
      tratar a falha como ausência de conteúdo.
- [x] Health, pausa, remoção e substituição das portas são testados.
- [x] CTest Linux/Windows, suíte Python, validação híbrida e documentação
      passam.

## Protocolo operacional

Hipótese: autorização explícita, processamento regional e redator de texto
reduzem exposição de dados visuais sem eliminar a capacidade de registrar
contexto visual local.

- baseline: sensor que captura qualquer frame e publica texto OCR bruto;
- métricas operacionais: capturas sem autorização bloqueadas, bytes fora da
  região persistidos, ocorrências de texto bruto no payload, falhas OCR
  distinguidas de ausência e transições de health;
- ablação: remover autorização, região, redator e estado de indisponibilidade
  separadamente;
- falsificação: qualquer frame não consentido ser armazenado, pixel ou texto
  bruto aparecer no evento, região ser ignorada ou falha OCR virar observação
  negativa.

Estas métricas são operacionais e não constituem evidência cognitiva.
