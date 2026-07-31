---
id: SPEC-047
title: Cognitive Component Wiring and Contracts
status: draft
created: 2026-07-31
authors: [Julio]
---

# SPEC-047: Cognitive Component Wiring and Contracts

## 1. Motivação
Atualmente, as implementações C++ dos módulos cognitivos (ex: `episodic_memory.hpp`, `world_model.hpp`) são estáticas ou fortemente acopladas aos testes sintéticos. Para que o `CognitiveCoordinator` (SPEC-045) consiga instanciá-los e orquestrá-los como um pipeline limpo, eles precisam compartilhar uma interface base de injeção de dependência e respeitar um contrato estrito de entrada/saída (I/O Cognitive Vector).

## 2. Objetivo
Padronizar as interfaces dos componentes cognitivos promovidos, forçando-os a implementar abstrações base de pipeline (ex: `ICognitiveModule`), e definir o struct de repasse de contexto (`CognitiveContext`).

## 3. Escopo Positivo
- Extrair interfaces virtuais puras para os órgãos cognitivos principais no C++ (ex: `IEpisodicMemory`, `IWorldModel`).
- Criar a struct `CognitiveContext` contendo o histórico do pipeline do evento em andamento.
- Modificar os headers atuais em `cpp/core/` para herdar e implementar essas interfaces sem quebrar os testes unitários.

## 4. Escopo Negativo
- Não reescrever a lógica interna de nenhum componente.
- Não introduzir polimorfismo dinâmico abusivo que mate a performance do cache de CPU.

## 5. Aceite Mensurável
- A compilação do C++ deve passar de forma limpa.
- Todos os testes antigos devem continuar passando após a alteração da assinatura das classes para usarem `override`.
