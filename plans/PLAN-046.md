# Plano de Execução: SPEC-046 (Cognitive State Persistence)

## Objetivo
Implementar serialização/desserialização do estado vital da memória de curto prazo (buffers e orçamentos) no banco de dados, para garantir a sobrevivência de contexto a reboots da máquina ou updates do projeto.

## Etapas Verificáveis

### Etapa 1: Definição do Contrato
- Criar `contracts/schemas/cognitive_snapshot.schema.json`.
- Definir chaves de versão, timestamp, e buffers estruturados (sem PII em plain text).

### Etapa 2: Implementação C++ (Serialization)
- Modificar o `RuntimeHost` (C++) para possuir um método genérico `save_snapshot()`.
- Requisitar de cada módulo do ciclo cognitivo (`get_state()`) um JSON injetável.
- Ponto de validação: Teste unitário em C++ garantindo a geração de um JSON limpo e aderente ao schema.

### Etapa 3: Integração no Storage
- Embutir na tabela SQLite do `PrivacyStorage` uma entrada de chave-valor para o blob do Snapshot.
- Escrever `load_snapshot()` na inicialização do Runtime.
- Teste de Integração: Forçar kill process `abort()`, reiniciar, e validar que as variáveis internas recuperaram o estado anterior.

## Arquivos Prováveis
- `contracts/schemas/cognitive_snapshot.schema.json`
- `cpp/core/runtime_snapshot.hpp`
- `cpp/core/runtime_host.hpp` (modificado)
- `cpp/tests/snapshot_test.cpp`
