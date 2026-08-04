# Contratos de requisição das portas cognitivas

## Escopo

Este documento define os DTOs 1.0 aprovados para as fronteiras de episódio,
memória episódica, workspace, metacognição e decisão da SPEC-047. Os schemas
normativos estão em `contracts/schemas/`; os tipos C++ em
`cpp/core/contracts/cognitive_port_requests.hpp` implementam essas fronteiras.

Os DTOs são aditivos. As assinaturas anteriores continuam compiláveis até a
migração coordenada pela SPEC-045, mas adapters concretos retornam falha
estruturada quando a entrada legada não contém os campos exigidos. Nenhum
adapter pode completar a entrada com episódio, hipótese, tensão, timestamp,
evidência ou contexto inventado.

## Versão e envelope

Todos os DTOs desta fronteira usam `schema_version: "1.0"`. Sucesso ou falha de
delegação é transportado por `PortResult<T>` 1.0. Dados de domínio inválidos ou
uma chamada legada insuficiente produzem `PortError`; ausência de observação não
é convertida em observação negativa.

## Operações

| Porta | Operação 1.0 | Entrada | Saída |
|---|---|---|---|
| `IEpisodeBoundaryPort` | `evaluate_observation` | `EpisodeObservationRequest` | `EpisodeUpdate` |
| `IMemoryWritePort` | `store_episode` | `EpisodeWriteRequest` | `MemoryWriteResult` |
| `IMemoryRetrievalPort` | `retrieve_memory` | `MemoryRetrievalRequest` | `MemoryRetrievalResponse` |
| `IWorkspaceSelectionPort` | `select_candidate` | `WorkspaceSelectionRequest` | `WorkspaceAssessment` |
| `IMetacognitionPort` | `evaluate_hypothesis` | `MetacognitionRequest` | `MetacognitivePortAssessment` |
| `ICognitiveDecisionPort` | `decide_evidence` | `DecisionRequest` | `CognitiveDecision` |

## Regras de mapeamento

- Observação de episódio preserva `event_id`, sessão, os dois relógios,
  aplicação, documento e modalidade. Buffers são separados por sessão.
- Escrita de memória exige um episódio completo e preserva contexto, qualidade,
  proveniência e embedding opcional. O adapter delega ao store real.
- Recuperação preserva filtros estruturados e retorna IDs, contexto, score e
  códigos de razão fornecidos pelo store em resposta 1.0 própria.
- Seleção do workspace preserva candidato, referências, conteúdo, sinais de
  saliência e relógios. A saída é uma projeção versionada do snapshot nativo e
  não contém o antigo campo artificial `tension`.
- Metacognição recebe hipótese e instante explícitos, preserva status,
  evidências, alternativas, verificação e proveniência, e devolve a avaliação
  associada à mesma hipótese. `workspace_snapshot_id`, quando presente, entra
  uma única vez como referência de evidência de suporte.
- Decisão recebe evidência e instante explícitos. `event_id` e
  `workspace_snapshot_id`, quando presente, são referências de linhagem sem
  duplicação; o orchestrator continua apenas propondo/suprimindo sugestões.

## Schemas executáveis

- `episode_port_request.schema.json`;
- `memory_retrieval_response.schema.json`;
- `workspace_port_request.schema.json`;
- `workspace_port_assessment.schema.json`;
- `metacognition_port_request.schema.json`;
- `metacognitive_port_assessment.schema.json`;
- `cognitive_decision_request.schema.json`.

Esta SPEC não conecta as novas operações ao `CognitiveCoordinator`, não remove
as assinaturas anteriores e não habilita atuadores. Essa migração permanece no
escopo da SPEC-045.
