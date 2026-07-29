# Contratos: sensor de áudio local

Os schemas executáveis são `audio_segment.schema.json` e
`audio_transcription.schema.json` em `contracts/schemas/`.

## Segmento

`audio.segment` contém `start_timestamp_ms`, `end_timestamp_ms`, a confiança
operacional do VAD e `audio_reference` (`uri`, `content_hash` e duração). O
evento referencia o áudio bruto sem duplicar seus bytes. `processing_cost_ms`
registra o custo medido de captura/VAD e da emissão do segmento.

## Transcrição

`audio.transcription` contém o mesmo intervalo temporal e o texto local com
confiança. Em caso de falha, `audio.transcription_failed` usa o mesmo contrato,
com `status: failed`, `text: null` e código de erro opcional. O segmento
anterior continua válido.

## Observabilidade e privacidade

Sem sinal, permissão ou fornecedor, o adaptador deve usar os estados do
`ObservationEnvelope` (`no_signal`, `not_observable` ou `sensor_failed`). Isso
não significa que fala não ocorreu. Apenas referências locais e hashes entram
na timeline; bytes e texto não são enviados para a nuvem.

Confiança é uma medida de qualidade do sensor. Não é probabilidade de verdade,
intenção ou emoção.
