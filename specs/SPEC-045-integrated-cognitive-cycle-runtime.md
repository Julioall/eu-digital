---
id: SPEC-045
title: Integrated Headless Cognitive Cycle
status: done
phase: design
dependencies: [SPEC-047, SPEC-023]
adrs: [ADR-0033]
contracts: [COGNITIVE_CYCLE_CONTRACTS.md, cognitive_cycle_input.schema.json, cognitive_cycle_result.schema.json, cognitive_cycle_stage.schema.json, port_invocation_context.schema.json, episode_segmentation_response.schema.json, observation_features.schema.json, salience_assessment.schema.json, hypothesis_formation.schema.json]
---

# SPEC-045 — Integrated Headless Cognitive Cycle

Status: done
Owner: humano  
Fase: design  
Dependências: SPEC-047 (Component Wiring), SPEC-023 (Pluggable Capability Runtime)  
ADRs aplicáveis: ADR-0033
Contratos afetados: Novo DTO `CognitiveCycleResult`.

## Problema
Os módulos cognitivos do projeto ("órgãos") operam de maneira isolada em testes sintéticos. O `RuntimeHost` captura eventos no `EventBus` mas termina no SQLite, sem uma orquestração de fluxo ("sistema circulatório"). A ausência desse motor coordenador impede que percepções gerem reações. A implementação anterior proposta sugeria classes estritas, o que quebra o desacoplamento de plugins.

## Objetivo
Criar o `CognitiveCoordinator` acoplado apenas às portas de serviço (`IPredictionPort`, `IMemoryWritePort`, etc) definidas na SPEC-047. O coordenador roteará os eventos recebidos no `EventBus` através destas portas, mantendo gestão rigorosa de estado (`queued`, `processing`, `degraded`, `completed`, `failed`), timeout cooperativo, backpressure, idempotência e tratamento estruturado de erros.

## Resultado observável
A emissão de um `CanonicalEvent` pelo sistema operacional resultará em um registro de log unificado do `CognitiveCoordinator` detalhando por quais portas o evento passou, em qual estado finalizou e se houve falha pontual tratada.

## Requisitos funcionais
- Assinar o `EventBus`.
- Resolver portas via `CapabilityRegistry`.
- Conduzir o pipeline: Segmentação -> Memória -> Predição -> Workspace -> Metacognição -> Self Model -> Decisão.
- Capturar erros de adaptadores sem derrubar o processo; se uma porta falha, o erro é englobado em um pacote de `DegradedState` e o pipeline prossegue se possível, ou encerra se crítico.
- Implementar política de *backpressure* (descartar/enfileirar eventos se o loop principal estiver sobrecarregado).
- Implementar idempotência para evitar *loops* internos (re-entrada de eventos gerados por ações).

## Requisitos não funcionais
- **Concorrência:** Não deve bloquear a thread principal da UI. 
- **Resiliência:** Timeouts de portas devem usar primitivas cooperativas seguras (ex: cancellation tokens ou timeouts na promessa de futuro), nunca abortar violentamente uma thread alheia.

## Entradas
- `CanonicalEvent` capturado do `EventBus`.

## Saídas
- `CognitiveDecision` DTO ao final do pipeline.
- Telemetria local estruturada (estado do ciclo).

## Fluxo
1. Evento chega e entra no estado `queued`.
2. O Loop de orquestração apanha o evento, muda para `processing`.
3. Chama `IEpisodeBoundaryPort`. Passa para `IMemoryWritePort`.
4. Se alguma porta não resolve, muda para estado `degraded`, omitindo a etapa.
5. Se falha fatal, entra em `failed`.
6. Termina gerando a `CognitiveDecision`, estado `completed`.

## Estados e transições
- `queued` -> `processing`
- `processing` -> `completed` (Sucesso pleno)
- `processing` -> `degraded` (Falha parcial em uma porta não-crítica) -> `completed` (ou `failed`)
- `processing` -> `failed` (Erro crasso, ex: OOM, exception não tratada no núcleo)
- `queued` -> `discarded` (Backpressure / Fila cheia / Duplicata)

## Erros esperados
- `CoordinatorTimeoutError`: Uma porta extrapolou a quota de tempo e a operação foi cancelada.
- `CycleDegradedError`: Adicionado à payload do evento quando falta resolução de capacidade.

## Escopo negativo
- Não invocar a UI ou modelos visuais (tratado em SPECs futuras).
- Não gerenciar Snapshotting aqui (SPEC-046 cuida disso).
- Não embutir lógica matemática de surpresa (pertence ao WorldModel).

## Critérios de aceite
- [x] O coordenador resolve dependências via CapabilityRegistry e não possui inclusões (`#include`) dos headers concretos de memória/modelo de mundo.
- [x] Eventos injetados sequencialmente acima do limite configurado de *backpressure* resultam em estado `discarded` e não derrubam o processo via OOM.
- [x] Falha forçada simulada no adaptador de predição gera estado final `degraded`, mantendo o fluxo até a decisão.
- [x] O design documentado inclui *baseline*, *ablação* (funcionar sem Módulo X) e *critério de falsificação*.

## Plano de testes

### Unitários
- Mockar TODAS as portas. Comprovar a sequência de chamadas.
- Simular timeout em MockPort e verificar cancelamento.

### Integração
- Levantar o coordenador com 2 adaptadores reais e os demais mockados. Injetar evento e conferir log de estado.

### Contrato
- O objeto de estado do coordenador reflete os enumeradores exatos do diagrama.

### Desempenho
- O overhead de agendamento na fila não ultrapassa 1ms.

### Recuperação
- Se um componente não está registrado, o ciclo segue graciosamente (baseline de ablação provada).

## Migração
Nenhuma migração externa.

## Rollback
Desligar a instanciação do `CognitiveCoordinator` no `RuntimeHost` através de uma flag de feature (`enable_cognitive_coordinator=false`).

## Evidências de conclusão
- Log de execução de CTest com a suíte de coordenação 100% aprovada.
- Relatório de Validação Científica da arquitetura (conforme AGENTS.md).
