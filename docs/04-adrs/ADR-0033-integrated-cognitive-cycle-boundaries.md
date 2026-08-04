# ADR-0033 — Fronteiras do ciclo cognitivo integrado

Status: accepted
Date: 2026-08-04
Accepted: 2026-08-04
Decision authority: delegação explícita do responsável humano para o agente
tomar as decisões necessárias do projeto sem nova intervenção

## Contexto

A SPEC-047 criou portas e DTOs que delegam fielmente aos componentes
promovidos. A implementação existente da SPEC-045 ainda chama assinaturas
legadas e recebe um `CanonicalEvent` C++ que não representa o schema
compartilhado completo. O pipeline também não possui fontes contratuais para o
episódio completo, features numéricas, saliência ou hipótese exigidos pelas
etapas seguintes.

Executar transformações ad hoc no coordenador voltaria a fabricar dados,
misturaria mecanismos cognitivos com orquestração e impediria ablação por
capacidade. O resultado atual também contém IDs/texto de UI inventados e pode
reentrar recursivamente pelo mesmo `EventBus`.

## Decisão proposta

1. Ativar a SPEC-045 somente após aceitação desta ADR e publicação dos schemas
   compartilhados descritos abaixo.
2. O coordenador recebe `CognitiveCycleInput` 1.0, contendo referência ao
   `CanonicalEvent` 1.0 completo, modo (`live` ou `replay`), deadline e cadeia
   causal. Um adapter legado só pode produzir essa entrada quando todos os
   campos obrigatórios existem; caso contrário retorna erro estruturado.
3. O coordenador não extrai features, calcula saliência nem forma hipóteses.
   Essas transformações são capacidades opcionais e substituíveis:
   `IObservationFeaturePort`, `ISalienceAssessmentPort` e
   `IHypothesisFormationPort`. Ausência significa etapa omitida e registrada,
   nunca valor zero ou hipótese negativa.
4. A fronteira de episódio passa a expor um resultado 1.0 com
   `EpisodeUpdate` e episódio materializado opcional. A memória só é chamada
   quando existe episódio completo válido; boundary parcial não vira episódio.
5. `IPatternLearningPort` recebe somente features numéricas publicadas por
   `IObservationFeaturePort`. Nenhum parsing livre de payload ou catálogo de
   tarefas é permitido no coordenador. A implementação real da extração exige
   hipótese, baseline, métrica, ablação e falsificação próprios.
6. Predição, workspace, metacognição e decisão são condicionais aos DTOs
   válidos produzidos pelas etapas anteriores. O coordenador registra
   `skipped_absent_input` sem converter ausência em falha cognitiva.
7. Toda invocação recebe `PortInvocationContext` 1.0 com deadline monotônico,
   `stop_token`, modo de replay e correlation ID. Novos overloads são aditivos;
   implementações sem cooperação são marcadas `timeout_uncooperative`,
   retiradas do ciclo e nunca têm thread abortada à força.
8. `CognitiveCycleResult` 1.0 contém estado final, etapas ordenadas, valores ou
   erros de porta, referências de evidência, decisão opcional e tempos
   operacionais. Não contém card, atividade, texto renderizado ou ação.
9. Eventos de resultado usam origem interna e cadeia causal. O `RuntimeHost`
   não reenvia eventos cujo tipo é `cognitive.cycle.result` ao coordenador. O
   ID idempotente deriva do evento de entrada e da versão da política; uma
   duplicata gera `discarded_duplicate`, sem novo resultado.
10. A fila é limitada e a política inicial é `drop_newest_v1`, observável por
    resultado `discarded_backpressure`. Replay usa a mesma ordem sem publicar
    decisão, UI ou ação.

## Contratos a publicar na implementação

- `cognitive_cycle_input.schema.json`;
- `cognitive_cycle_result.schema.json`;
- `cognitive_cycle_stage.schema.json`;
- `port_invocation_context.schema.json`;
- `episode_segmentation_response.schema.json`;
- `observation_features.schema.json`;
- `salience_assessment.schema.json`;
- `hypothesis_formation.schema.json`.

Os schemas em `contracts/` são normativos. Representações C++ devem ser
verificadas contra fixtures compartilhadas; não haverá um segundo schema C++.

## Hipótese operacional e avaliação

Hipótese `H-SPEC045-ORCHESTRATION`: uma fila limitada e um pipeline por portas
versionadas preservam proveniência e produzem resultado determinístico sem
reentrada, mesmo quando cada capacidade opcional está ausente ou falha.

- Baseline: `pass_through_audit_v0`, que valida a entrada e produz silêncio
  auditável sem invocar capacidades cognitivas opcionais.
- Ablação: remover individualmente cada porta pelo `CapabilityRegistry` usando
  a mesma entrada e comparar etapas executadas/omitidas.
- Métricas operacionais: latência de fila e ciclo p50/p95/p99, profundidade
  máxima, memória máxima, contagem de duplicatas/reentrada, deadlines violados
  e determinismo de replay.
- Falsificação: a hipótese falha se uma entrada gerar mais de um ciclo, se a
  fila ultrapassar o limite, se replay divergir semanticamente, se ausência
  produzir dado inventado, se uma falha isolada derrubar o processo ou se um
  timeout bloquear shutdown além da quota documentada.

Essas métricas provam apenas segurança e determinismo da orquestração. Não são
evidência de aprendizagem, utilidade ou validade cognitiva.

## Consequências

- A SPEC-045 cresce de sete para onze fronteiras, mas cada mecanismo permanece
  removível e testável por ablação.
- O produto pode operar no baseline sem sensores ou mecanismos opcionais, com
  silêncio e limitações explícitas.
- Formação de hipótese, extração de features e saliência não são escondidas em
  glue code e precisam de promoção científica própria antes de disponibilidade
  no produto.
- SPEC-046 poderá reaplicar `CognitiveCycleInput` em modo replay sem efeitos
  externos.

## Alternativas rejeitadas

- Usar `payload` textual como sessão, timestamp, features ou hipótese: perde
  proveniência e fabrica semântica.
- Criar uma hipótese diretamente de toda predição: predição não é evidência
  suficiente e possui contrato diferente.
- Atribuir saliência constante ou defaults silenciosos: ausência não é zero.
- Executar cada porta em thread destacada para simular timeout: deixa trabalho
  órfão e torna shutdown inseguro.
- Filtrar reentrada apenas por prefixo de ID: IDs não são classificação causal.

## Reversão

Antes da aceitação, nenhum código dependia desta ADR. A integração pode ser
desativada pela feature flag
`enable_cognitive_coordinator=false`, preservando timeline e portas isoladas.
