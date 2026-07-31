---
id: SPEC-047
title: Component Wiring, Ports and Contracts
status: draft
phase: design
dependencies: [SPEC-023, SPEC-033, SPEC-034, SPEC-035, SPEC-036, SPEC-037, SPEC-038, SPEC-039]
adrs: []
contracts: []
---

# SPEC-047 — Component Wiring, Ports and Contracts

Status: draft  
Owner: humano  
Fase: design  
Dependências: SPEC-023 (Pluggable Capability Runtime), SPECs 033-039 (Native Promotions)  
ADRs aplicáveis: Nenhuma  
Contratos afetados: Novos contratos C++ internos.

## Problema
Atualmente, as implementações C++ dos módulos cognitivos promovidos são fortemente acopladas a tipos concretos e não oferecem pontos de injeção polimórficos seguros para um orquestrador. Modificar as classes concretas diretamente quebra a estabilidade e viola a SPEC-023. O orquestrador central não deve conhecer `EpisodicMemory` ou `WorldModel` como instâncias concretas, mas sim operar sobre portas de serviço.

## Objetivo
Definir o conjunto de Portas virtuais (Interfaces em C++) que representam as funções cognitivas isoladas (ex: `IEpisodeBoundaryPort`, `IMemoryWritePort`) e implementar o padrão *Adapter* ao redor dos componentes promovidos atuais para satisfazer essas portas através do `CapabilityRegistry`. Definir também as *Data Transfer Objects* (DTOs) imutáveis que fluirão entre os módulos.

## Resultado observável
O sistema será capaz de registrar, via `CapabilityRegistry`, instâncias dos adaptadores (que encapsulam os módulos concretos). Um coordenador poderá invocar `IMemoryWritePort::write()` sem saber que tipo de armazenamento em memória está por trás.

## Requisitos funcionais
- Criar a camada de Portas Virtuais Puras C++ correspondentes a cada função cognitiva promovida.
- Criar Adaptadores C++ que herdam das portas virtuais e delegam a chamada aos módulos concretos promovidos.
- Substituir a ideia do "objeto Deus" (`CognitiveContext`) por retornos estritos e imutáveis em cadeia (ex: `PredictionAssessment`, `WorkspaceCandidateSet`).
- Integrar as portas ao mecanismo de resolução de capacidades (SPEC-023).

## Requisitos não funcionais
- **Estabilidade:** Os componentes promovidos originais (ex: `cpp/core/episodic_memory.hpp`) não devem ser modificados de forma a quebrar seus testes unitários atuais.
- **Desempenho:** O *overhead* das chamadas polimórficas virtuais deve ser mínimo (evitar *pointer chasing* desnecessário e *cache misses* extremos).

## Entradas
- Eventos e requisições puras via DTOs C++ (`CanonicalEvent`).
- Registro explícito de capacidades na subida do Runtime.

## Saídas
- Contratos tipados imutáveis: `EpisodeUpdate`, `MemoryWriteResult`, `MemoryRetrievalResult`, `PredictionAssessment`, `WorkspaceSnapshot`, `MetacognitiveAssessment`, `SelfConstraintSnapshot`, `CognitiveDecision`.

## Fluxo
1. Subida do `RuntimeHost`.
2. O `CapabilityRegistry` registra as portas (ex: `IPredictionPort` implementada por `WorldModelAdapter`).
3. O Coordenador solicita as portas ao Registry.
4. O Coordenador invoca os métodos das portas passando DTOs imutáveis.

## Estados e transições
- `Registered`: A porta foi resolvida no registry.
- `Unresolved`: A porta não está disponível (degradação).
- `Failed`: A porta lançou uma exceção, levando o adaptador a retornar um `Result` com falha estruturada.

## Erros esperados
- `CapabilityNotResolvedError`: Tentativa de acessar uma função sem adaptador registrado.
- `AdapterDelegationError`: Falha na classe subjacente mapeada para o fluxo principal.

## Escopo negativo
- Não modificar a semântica interna ou o layout de memória de `EpisodicMemory`, `WorldModel` etc.
- Não usar `CognitiveContext` mutável entre etapas.
- Não invocar a persistência de disco na camada de portas.

## Critérios de aceite
- [ ] A arquitetura C++ deve possuir pelo menos 8 portas de abstração distintas mapeadas.
- [ ] Nenhuma classe promovida (ex: `episodic_memory.hpp`) foi alterada para herdar dessas portas novas.
- [ ] A compilação CMake (`core`) é bem-sucedida sem dependências cíclicas.
- [ ] Testes de ausência: Solicitar uma porta não registrada retorna estado de ausência controlada.
- [ ] Testes de substituição: Possibilidade de instanciar um `MockPredictionPort` e trocá-lo pelo real dinamicamente via registro.

## Plano de testes

### Unitários
- Testar cada adaptador para confirmar que o DTO de entrada é mapeado perfeitamente para as chamadas dos métodos nativos do componente isolado.
- Validar ausência, remoção dinâmica, reinstalação e substituição (`AGENTS.md`).

### Integração
- Registrar 3 adaptadores no `CapabilityRegistry` e recuperá-los com sucesso.

### Contrato
- Verificar se todos os DTOs respeitam regras de imutabilidade (sem mutadores públicos).

### Desempenho
- A chamada via Adapter virtual não deve degradar a execução em mais de 1% em microbenchmarks em relação à chamada concreta direta.

### Recuperação
- Se um componente promovido lançar `std::runtime_error`, o adaptador deve capturar e retornar como um estado falho na DTO resultante, sem derrubar a aplicação.

## Migração
- Inserir a fase de registro dos adaptadores no método `start()` ou construtor do `RuntimeHost`.

## Rollback
- Reverter o PR, já que a alteração apenas adiciona arquivos de abstração e injeta no Runtime, sem alterar o banco de dados.

## Evidências de conclusão
- Relatório de cobertura de testes mostrando os novos arquivos de interfaces e adaptadores.
- Código no `main` sem violação das regras estruturais do C++.
