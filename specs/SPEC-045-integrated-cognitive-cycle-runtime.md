---
id: SPEC-045
title: Integrated Cognitive Cycle Runtime
status: draft
created: 2026-07-31
authors: [Julio]
---

# SPEC-045: Integrated Cognitive Cycle Runtime

## 1. Motivação
Apesar de o projeto "Eu Digital" possuir uma vasta biblioteca de módulos cognitivos altamente testados (segmentador de episódios, modelo de mundo, memória episódica, metacognição, etc.), atualmente não existe um "coração" que impulsione o fluxo de dados sequencialmente por esses módulos no runtime C++ (`RuntimeHost`). A ausência deste fluxo central impede que o sistema funcione como um produto utilizável, relegando-o a um laboratório de testes isolados.

## 2. Objetivo
Implementar um **CognitiveCoordinator** dentro do `RuntimeHost` responsável por capturar eventos do `EventBus` e conduzi-los ordenadamente através do ciclo cognitivo completo, alimentando cada órgão com o contexto do anterior, de forma estritamente serial ou estruturadamente assíncrona, respeitando os contratos firmados de cada módulo.

## 3. Escopo

### 3.1 Escopo Positivo
- Criar a classe `CognitiveCoordinator` no C++ (e sua referência em Python).
- Assinar o `EventBus` e reagir a novos `CanonicalEvent`s.
- Executar, em ordem estrita: `EpisodeSegmenter` → `EpisodicMemory` → `PatternLearner` → `WorldModel` → `GlobalWorkspace` → `MetacognitionCuriosity` → `FunctionalSelfModel` → `SuggestionOrchestrator`.
- Gerenciar o repasse do `Context` de um módulo para outro (ex: `MemoryRetrieval` repassada ao `WorldModel`).
- Gerenciar a política de timeout de cada etapa da cadeia.
- Tratar e engolir erros de órgãos individuais sem causar crash no ciclo inteiro.

### 3.2 Escopo Negativo
- Não implementar lógicas internas cognitivas novas (apenas amarrar as existentes).
- Não persistir o estado cognitivo em disco neste componente (tratado na SPEC-046).
- Não lidar com interface gráfica ou empacotamento.
- Não executar chamadas em nuvem (preservar 100% execução local).

## 4. Dependências e Contratos
- Depende de: `EventBus`, `TimelineStore`, e todos os módulos promovidos (SPEC-033 a SPEC-039).
- Modifica: `RuntimeHost` (para instanciar o Coordinator).
- Contratos Envolvidos: `canonical_event.schema.json`. Novo contrato: `cognitive_cycle_state.schema.json` (apenas para logging).

## 5. Fluxo de Dados e Arquitetura

O ciclo cognitivo deve operar preferencialmente através de um pipeline orientado a Eventos Internos (Internal Bus) ou chamadas síncronas bloqueantes dentro de uma Task Background para garantir determinismo.

```text
EventBus (CanonicalEvent)
  │
  ├─> 1. EpisodeSegmenter (Avalia quebra de episódio)
  │     └─> Se novo: EpisodicMemory (Armazena)
  │
  ├─> 2. WorldModel (Calcula erro de previsão/surpresa sobre o evento)
  │
  ├─> 3. PatternLearner (Registra repetições em background)
  │
  ├─> 4. GlobalWorkspace (Funde Evento, Memórias passadas e Surpresa)
  │
  ├─> 5. Metacognition (Calcula a relevância/confiança do Workspace)
  │
  ├─> 6. FunctionalSelfModel (Consulta capacidades/limitações)
  │
  └─> 7. SuggestionOrchestrator (Gera silêncio, pergunta ou sugestão)
```

## 6. Tratamento de Falhas e Degradação
Se um módulo (ex: `PatternLearner`) disparar uma exceção de OOM ou Timeout (demorar mais de 50ms), o `CognitiveCoordinator` deve abortar a etapa desse módulo, injetar um erro "Graceful Degradation" no `GlobalWorkspace`, e prosseguir para as próximas etapas para não interromper a percepção do usuário.

## 7. Critérios de Aceite Mensuráveis
1. Um evento publicado no `EventBus` deve atravessar a cadeia dos 7 módulos base sem causar memory leaks (testado via `valgrind`/ASan).
2. O tempo máximo para processamento de um ciclo síncrono (excluindo I/O do modelo local) não deve exceder `200ms`.
3. Erros induzidos (Fault Injection) em 3 módulos simultaneamente não podem causar travamento do loop principal.
4. Deve haver equivalência Python-C++ na sequência de logs gerada por um evento atravessando a cadeia.

## 8. Testes Exigidos
- **Testes Unitários:** O Coordinator reage ao evento e chama as interfaces.
- **Teste de Integração:** O Coordinator instanciado com instâncias mockadas (via GMock) de todos os módulos cognitivos, verificando a ordem estrita de chamada.
- **Teste Metamórfico:** Atrasar um módulo em X ms não deve afetar o output final, apenas a latência (salvo timeouts).

## 9. Impacto no Produto
Esta é a SPEC que transforma o "Laboratório" em "Produto", pois faz a máquina finalmente pulsar a cada clique de mouse capturado.
