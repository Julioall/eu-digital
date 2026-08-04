---
id: SPEC-048
title: Structured Cognitive Output and Dialogue
status: done
phase: design
dependencies: [SPEC-045, SPEC-040, SPEC-042]
adrs: [ADR-0035]
contracts: [COGNITIVE_OUTPUT_CONTRACTS.md, cognitive_output_request.schema.json, language_rendering_candidate.schema.json, cognitive_output.schema.json]
---

# SPEC-048 — Structured Cognitive Output and Dialogue

Status: done
Owner: humano  
Fase: design  
Dependências: SPEC-045 (Integrated Cycle), SPEC-040 (Local Model), SPEC-042 (Avatar Shell)  
ADRs aplicáveis: ADR-0035
Contratos afetados: `COGNITIVE_OUTPUT_CONTRACTS.md`,
`cognitive_output_request.schema.json`,
`language_rendering_candidate.schema.json` e `cognitive_output.schema.json`.

## Problema
A implementação original de decisão no projeto misturava as responsabilidades de decidir falar, construir o prompt do modelo e invocar a interface gráfica em um único módulo maciço (`DecisionOutputRouter`). Isso violava o princípio de responsabilidade única e tornava inviável tratar timeouts linguísticos ou falhas sem corromper a decisão raiz. Além disso, as interrupções proativas concorriam com respostas ativas do usuário pelo mesmo orçamento.

## Objetivo
Estruturar o final do pipeline cognitivo em quatro estágios desacoplados:
1. `CognitiveDecisionPolicy`: Define a intenção (silêncio, pergunta, resposta_solicitada, sugestao_proativa).
2. `CognitiveOutputRequest`: DTO puramente semântico da decisão.
3. `LocalLanguageRenderer`: Responsável por traduzir o DTO via engenharia de prompt (com fallback).
4. `PresentationPort`: Envia para fora (Avatar Qt).

## Resultado observável
Ao ocorrer uma interação, o log mostrará a intenção desvinculada do texto gerado (ex: Intenção: `sugestao_proativa`. Texto Gerado: `Parece que você está focando no código, deseja silenciar as notificações?`).

## Requisitos funcionais
- Distinguir logicamente `resposta_solicitada` (não gasta orçamento proativo diário) de `sugestao_proativa` (limitada pelo `SuggestionOrchestrator`).
- Incorporar o Snapshot de Restrições (Self Model) no prompt do `LocalLanguageRenderer`.
- Forçar o modelo local a retornar um formato estruturado que pode ser decodificado com segurança.
- Se o modelo demorar (timeout), ou retornar vazio/lixo gerado, o `LocalLanguageRenderer` retorna um texto de "Fallback" seguro ou devolve Silêncio, sem engasgar o pipeline principal.
- Não invocar implementações Qt diretamente.

## Requisitos não funcionais
- **Segurança de Geração:** Bloqueio de alegações factuais estritas no system prompt; forçar referências a memórias contidas apenas no `CognitiveOutputRequest`.
- **Desempenho:** Timeout estrito e cancelável para a geração do modelo.

## Entradas
- O pacote do `CognitiveCoordinator` ao final do loop (Contendo memórias e assessments).

## Saídas
- `ValidatedDialogueOutput` encaminhado para a `PresentationPort`.

## Fluxo
1. `CognitiveDecisionPolicy` avalia Workspace. Se silêncio, FIM.
2. Se decidir falar, constrói `CognitiveOutputRequest`.
3. Chama `LocalLanguageRenderer`. Se modelo offline/lento, usa Fallback string (se crítica) ou Silêncio.
4. Repassa ao `PresentationPort`.
5. Interface recebe.

## Estados e transições
- `rendered`: Modelo local processou e validou a resposta.
- `malformed`: Resposta do LLM falhou no parse JSON ou feriu bloqueios.
- `timeout`: Tempo expirou.
- `fallback_used`: Uma frase pré-pronta foi usada devido a falha no LLM.

## Erros esperados
- Erros do LLM (`malformed`, `timeout`) devem ser absorvidos pela camada de Renderização.

## Escopo negativo
- Não manter histórico conversacional ad-hoc aqui.
- Não alterar a implementação do QML (apenas usar a Porta).

## Critérios de aceite
- [x] A arquitetura C++ cria as interfaces `ILanguageRenderer` e `IPresentationPort`.
- [x] Respostas a perguntas do usuário ignoram e não debitam o limite configurado de sugestões proativas (Cooldown não sofre reset).
- [x] O modelo local possui Fallback forçado caso demore mais de `N` ms, não travando a thread coordenadora.
- [x] Todos os outputs do `LocalLanguageRenderer` são validados contra um schema rígido para evitar alucinações de formato.

## Plano de testes

### Unitários
- Injetar `MockLanguageRenderer` que retorna lixo. O módulo deve converter para estado de erro ou fallback.
- Injetar requisição de "resposta solicitada". Orçamento deve permanecer intacto.

### Integração
- Integrar com o modelo simulado e testar a geração do prompt system injetado com o *Self Constraint Snapshot*.

### Contrato
- Validar `cognitive_output.schema.json`.

### Desempenho
- O roteamento e decisão não podem demorar mais que 1ms; a renderização será assíncrona.

### Recuperação
- Coberto pelo Fallback.

## Migração
- Sem migrações de DB.

## Rollback
- Reverter o PR caso o LLM cause segfaults externos.

## Evidências de conclusão
- Log de "Fallback used due to LLM timeout".
