# Plano de Execução: SPEC-047 (Cognitive Component Wiring and Contracts)

## Objetivo
Estruturar o projeto com interfaces polimórficas (C++) e protocolos (Python) base para permitir que o Coordenador injete as dependências de forma limpa e acoplável.

## Etapas Verificáveis

### Etapa 1: Definição Base
- Criar `cpp/core/cognitive_context.hpp` definindo a estrutura de passagem de contexto.
- Criar `cpp/core/icognitive_module.hpp` com as classes virtuais puras.

### Etapa 2: Refatoração dos Headers
- Modificar `episode_segmenter.hpp` para implementar `IEpisodeSegmenter`.
- Repetir para `episodic_memory.hpp`, `pattern_learner.hpp`, `global_workspace.hpp`, `world_model.hpp`, `metacognition_curiosity.hpp`, `functional_self_model.hpp`.

### Etapa 3: Validação de Build
- Rodar o CMake e verificar dependências quebradas.
- Rodar o CTest para garantir que o comportamento funcional não mudou.

## Arquivos Prováveis
- `cpp/core/cognitive_context.hpp`
- `cpp/core/icognitive_module.hpp`
- (Headers modificados de cada módulo)
