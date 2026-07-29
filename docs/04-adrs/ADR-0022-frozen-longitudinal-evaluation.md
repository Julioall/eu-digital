# ADR-0022 — Avaliação longitudinal com protocolo e snapshots congelados

Status: aceito
Data: 2026-07-29

## Contexto

A SPEC-022 precisa comparar retenção, calibração, ganhos, perdas e deriva do
self-model em 7, 30 e 90 dias. O harness da SPEC-018 já separa métricas
cognitivas e operacionais, mas não define um artefato versionado que congele o
protocolo, vincule o holdout e permita reconstruir o relatório somente a partir
de snapshots.

## Decisão

Adicionar uma referência Python local com:

- `LongitudinalProtocol`, congelado por hash determinístico e vinculado ao hash
  do holdout;
- `LongitudinalSnapshot`, imutável, com checkpoint, métricas separadas,
  versão/digest do self-model e referências de origem;
- `LongitudinalReport`, reconstruível pela mesma sequência de snapshots, com
  curva de retenção, comparação contra o baseline cronológico, ganhos/perdas,
  métricas de calibração e drift quantificado do self-model.

Somente os checkpoints 7, 30 e 90 são aceitos pela configuração inicial. O
relatório não reestima métricas nem altera critérios depois de um protocolo
congelado. Nenhuma observação é inferida quando um snapshot ou métrica está
ausente.

## Protocolo científico

- baseline: `chronological_first_snapshot_v0`;
- métricas: retenção, calibração, estabilidade, contradição e deriva de
  self-model, mantendo métricas operacionais separadas;
- ablação: reconstruir o mesmo relatório sem uma fonte/módulo e comparar os
  snapshots correspondentes;
- falsificação: o relatório não é reproduzível dos snapshots, ou a comparação
  longitudinal não distingue ganhos/perdas e drift conhecido;
- holdout: o protocolo exige hash SHA-256 e registro de acesso antes de ser
  congelado.

## Consequências

Positivas:

- resultados podem ser reproduzidos sem reexecutar captura;
- mudanças de métrica ficam vinculadas a um protocolo e holdout explícitos;
- a versão do self-model é quantificada sem alegar identidade fenomenal.

Custos:

- snapshots precisam ser coletados nos três checkpoints;
- métricas ausentes permanecem ausentes e podem deixar uma curva incompleta;
- validade ecológica exige sessões longitudinais reais posteriores.

## Reversão

Desabilitar o avaliador não altera eventos, memória ou self-model. Snapshots são
artefatos de avaliação; remover a avaliação não apaga suas fontes nem modifica
o estado cognitivo.
