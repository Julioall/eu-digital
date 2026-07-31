# Plano de Execução: SPEC-045 (Integrated Cognitive Cycle Runtime)

## Objetivo
Implementar o coordenador central (`CognitiveCoordinator`) responsável por rotear eventos do `EventBus` através de todos os módulos cognitivos isolados, transformando-os em um fluxo de vida utilizável.

## Etapas Verificáveis

### Etapa 1: Implementação Python (Referência)
- Criar `python/eu_digital_lab/cognitive_coordinator.py`.
- Instanciar a versão em Python do coordenador, injetando mocks ou instâncias falsas de cada um dos órgãos cognitivos.
- Escrever testes em `python/tests/test_cognitive_coordinator.py` provando a ordem das chamadas.
- Ponto de validação: `pytest` passando.

### Etapa 2: Definição do Contrato C++ e Structs
- Adicionar chaves auxiliares ao schema `canonical_event.schema.json` se necessário.
- Criar `cpp/core/cognitive_coordinator.hpp`.

### Etapa 3: Implementação do Loop C++ e Delegação
- Escrever o loop principal do `CognitiveCoordinator` no C++ usando injeção de dependência via interfaces puras (`std::shared_ptr<IEpisodicMemory>`, etc).
- Implementar try/catch ao redor de cada chamada para garantir o "Graceful Degradation".
- Ponto de validação: Testes unitários com GMock em `cpp/tests/cognitive_coordinator_test.cpp`.

### Etapa 4: Integração no RuntimeHost
- Em `cpp/core/runtime_host.hpp`, instanciar o `CognitiveCoordinator`.
- Ligar o output do `EventBus` na entrada do `CognitiveCoordinator`.
- Rodar o `eu_digital_runtime --run` passando a fixture de teste e verificar via logs se a cascata ocorre do sensor à saída.

## Arquivos Prováveis
- `python/eu_digital_lab/cognitive_coordinator.py`
- `cpp/core/cognitive_coordinator.hpp`
- `cpp/core/runtime_host.hpp`
- `cpp/tests/cognitive_coordinator_test.cpp`

## Dependências
- Módulos C++ existentes.
- Contrato da SPEC-047 (Wiring) para injetar as interfaces puras.
