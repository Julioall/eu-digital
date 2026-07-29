# ADR-0017 — Sensor de áudio local por portas e falha isolada

Status: aceito
Data: 2026-07-29

## Contexto

A SPEC-015 precisa detectar fala e produzir transcrições temporais sem
acoplar o runtime a um microfone, codec, biblioteca de VAD ou modelo de fala.
O áudio é dado sensível e uma falha de transcrição não pode apagar a
observação temporal nem derrubar o barramento.

## Decisão

Implementar o sensor em C++ por três portas locais substituíveis:

- `AudioCapture`, que fornece frames e referências locais ao áudio bruto;
- `VoiceActivityDetector`, que classifica frames sem afirmar conteúdo semântico;
- `LocalSpeechToText`, que transcreve um segmento somente quando solicitado.

O sensor emitirá primeiro um evento `audio.segment` com timestamps, confiança
de VAD, referência ao áudio e custo medido. A transcrição será uma operação
isolada: sucesso emite `audio.transcription`; falha emite
`audio.transcription_failed`, preservando o segmento e a timeline. Ausência de
sinal será reportada como estado explícito e não como fala ausente.

O descritor da capacidade publicará `observe.audio` e declarará os contratos
versionados. Nenhuma implementação concreta de hardware, codec ou modelo será
incluída no núcleo, e nenhuma integração externa ou envio de áudio será
adicionado.

## Privacidade e limites epistemológicos

- o evento contém somente URI e hash do áudio bruto, nunca os bytes;
- retenção e remoção do arquivo permanecem responsabilidade do adaptador local;
- confiança de VAD/transcrição é qualidade operacional, não evidência de
  verdade, intenção ou emoção;
- o sensor não infere verdade, intenção ou emoção pela voz.

## Consequências

Positivas:

- backends locais podem ser substituídos e testados por fixtures;
- segmentos temporais permanecem disponíveis quando STT falha;
- custo de processamento fica auditável por captura, VAD e transcrição;
- o núcleo continua operando sem a capacidade de áudio.

Custos:

- adaptadores concretos precisam implementar as portas e sua política de
  retenção;
- a qualidade da transcrição exige calibração posterior;
- a referência atual não escolhe modelo nem plataforma de áudio.

## Reversão

Desabilitar ou remover o plugin `local.audio_sensor` deixa os contratos e
eventos históricos legíveis e produz `not_observable` no envelope de observação.
Não é necessário alterar o event bus ou o núcleo cognitivo.
