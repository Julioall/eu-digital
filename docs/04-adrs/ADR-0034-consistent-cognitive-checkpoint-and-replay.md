# ADR-0034 — Checkpoint cognitivo consistente e replay sem efeitos

Status: accepted  
Date: 2026-08-04  
Accepted: 2026-08-04  
Decision authority: delegação explícita do responsável humano para o agente
tomar as decisões necessárias do projeto sem nova intervenção

## Contexto

A SPEC-046 possui um schema e persistência preliminares, mas o snapshot 1.0
aceita um `payload` arbitrário, o C++ não verifica seu checksum e o host apenas
conta eventos após o cursor. Nenhuma fronteira define quais módulos formam o
estado consistente, como restaurá-los sem estado parcial ou quando um snapshot
é completo. A timeline também não preserva separadamente todos os campos
temporais adicionados ao `CanonicalEvent` nativo.

Usar o blob existente como se fosse um checkpoint completo perderia estado de
aprendizagem anterior ao cursor. Restaurar módulos concretos dentro do host
quebraria a independência de capacidades. Reaplicar eventos de resultado ou
decisão criaria efeitos duplicados.

## Decisão

1. O snapshot 1.0 permanece legível apenas como artefato legado, mas não é
   elegível para restauração automática. A implementação normativa será
   `CognitiveSnapshot` 2.0 em schema próprio, sem alterar silenciosamente 1.0.
2. A timeline continua sendo a fonte da verdade. Um snapshot é apenas um
   acelerador e contém `last_applied_event_id`, instante, fingerprint da
   configuração, checksum e um `CognitiveStateBundle` versionado.
3. O bundle contém o estado idempotente do coordenador e fragmentos ordenados
   por `provider_id`. Cada fragmento usa entradas string→string versionadas;
   não contém imagem, áudio bruto, segredo, ação ou texto de UI.
4. Capacidades restauráveis implementam `ICognitiveStatePort`, resolvida pelo
   `CapabilityRegistry`. O núcleo não importa adapters ou módulos concretos.
   Captura e restauração retornam `PortResult<T>` estruturado.
5. Um snapshot só pode ser criado quando toda capacidade cognitiva ativa que
   mantém estado declara `supports_checkpoint=true` e possui fragmento
   correspondente. Se a completude não for demonstrável, o host não grava um
   snapshot parcial e usa replay integral.
6. A restauração valida integralmente versão, checksum, fingerprint,
   expiração, cursor e conjunto de providers antes de mutar módulos. Cada
   provider restaura atomicamente; se uma restauração falhar, o host reaplica
   os fragmentos capturados antes da tentativa e passa ao snapshot anterior.
7. O host tenta no máximo os dois snapshots mais recentes. Blob corrompido,
   DPAPI indisponível, versão incompatível, configuração diferente, provider
   ausente, cursor inexistente ou snapshot expirado levam ao anterior e depois
   ao cold replay integral.
8. Replay converte eventos da timeline em `CognitiveCycleInput` com
   `replay_mode=true`, preserva ordem e ignora eventos internos de resultado.
   Decisão, UI, ação e nova gravação de snapshot são proibidas durante replay.
9. O estado idempotente restaurado inclui os IDs já aplicados até o cursor.
   Eventos posteriores são reaplicados uma vez; duplicatas não geram segundo
   resultado terminal.
10. A captura de estado ocorre na fronteira serial do coordenador após um ciclo
    concluído. Somente a cópia profunda pode bloquear essa fronteira e possui
    orçamento de 5 ms. Serialização final, DPAPI e transação SQLite ocorrem em
    worker local limitado a um snapshot pendente, mantendo sempre o mais novo.
11. A política inicial captura a cada 100 eventos live e no encerramento limpo.
    Os intervalos e a expiração são configuração explícita, com defaults
    versionados e testes determinísticos.
12. A tabela SQLite usa transação `BEGIN IMMEDIATE`/`COMMIT`, preserva os dois
    últimos blobs e nunca é removida automaticamente em rollback. Uma migração
    aditiva da timeline preserva `occurred_at`, `received_at` e sessão do evento
    para reconstrução temporal fiel.
13. O teste de crash pode executar um binário auxiliar que chama `abort()` em
    subprocesso. O runtime de produção nunca provoca crash deliberado.
14. A integração é reversível por
    `RuntimeConfig.enable_cognitive_snapshots=false`; dados existentes não são
    apagados durante reversão.
15. O plaintext canônico possui limite inicial de 4 MiB, configurável no host.
    O worker rejeita snapshots maiores antes de invocar DPAPI; o limite deve ser
    revisto somente com evidência de benchmark, sem truncar estado silenciosamente.

## Contratos

- `cognitive_snapshot.schema.json` permanece como legado 1.0;
- `cognitive_snapshot_v2.schema.json` será o envelope normativo 2.0;
- `cognitive_state_bundle.schema.json` definirá coordenador e fragments;
- os DTOs C++ serão verificados contra fixtures compartilhadas.

## Hipótese operacional

`H-SPEC046-RECOVERY`: checkpoint completo mais replay posterior ao cursor
reconstrói o mesmo estado observável de uma execução contínua sem repetir
efeitos externos.

- Baseline: `full_timeline_replay_v0`, sem snapshot.
- Métricas: equivalência de estado, eventos reaplicados, tempo de recuperação,
  duração da cópia, bytes do blob, fallbacks e efeitos externos emitidos.
- Ablação: desabilitar snapshot mantendo o mesmo replay e a mesma timeline.
- Testes metamórficos: inserir snapshot corrompido ou incompatível antes de um
  snapshot válido não altera o estado final; repetir recovery não altera o
  resultado.
- Falsificação: perda/duplicação de evento, estado diferente da execução
  contínua, efeito externo em replay, captura acima de 5 ms, snapshot parcial
  aceito ou corrupção causando crash.

Essas métricas validam continuidade operacional. Não demonstram memória
fenomenal, identidade pessoal ou aprendizagem útil.

## Consequências

- Providers sem checkpoint continuam utilizáveis, mas obrigam cold replay.
- O snapshot v2 não depende de Python nem de serviço externo.
- A primeira implementação pode provar o fluxo com boundary de episódio e
  estado do coordenador; outros providers só habilitam aceleração quando seus
  próprios checkpoints forem implementados e registrados.
- A execução contínua e o cold replay permanecem controles normativos, não
  caminhos inferiores omitidos dos testes.

## Alternativas rejeitadas

- Serializar ponteiros ou objetos concretos: não é portável nem substituível.
- Aceitar payload 1.0 arbitrário: não prova completude nem restauração.
- Salvar somente `last_applied_event_id`: perde estado anterior ao cursor.
- Restaurar parcialmente e continuar: mistura épocas incompatíveis.
- Enviar snapshots para nuvem: viola localidade e privacidade.
- Abortar threads para capturar estado: viola segurança operacional.

## Reversão

Desabilitar snapshots e executar `full_timeline_replay_v0`. A tabela e blobs
permanecem intactos; exclusão continua sujeita à política explícita de gestão
de dados.
