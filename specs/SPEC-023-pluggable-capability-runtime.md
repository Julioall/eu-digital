---
id: SPEC-023
title: Runtime de capacidades removíveis
status: done
phase: 0
dependencies: [SPEC-001, SPEC-002]
adrs: [ADR-0001, ADR-0008, ADR-0009]
contracts: [CAPABILITY_DESCRIPTOR.md, CAPABILITY_STATE_SCHEMA.md, OBSERVATION_ENVELOPE.md, MODULE_LIFECYCLE.md, SELF_MODEL_SCHEMA.md]
---

# SPEC-023 — Runtime de capacidades removíveis

Status: done
Fase: 0
Dependências: SPEC-001, SPEC-002
ADRs aplicáveis: ADR-0001, ADR-0008, ADR-0009
Contratos afetados:
- `CAPABILITY_DESCRIPTOR.md`
- `CAPABILITY_STATE_SCHEMA.md`
- `OBSERVATION_ENVELOPE.md`
- `MODULE_LIFECYCLE.md`
- `SELF_MODEL_SCHEMA.md`

## Problema

O núcleo não pode depender de uma lista fixa de sensores, ferramentas, atuadores ou modelos. Deve continuar funcionando quando módulos forem adicionados, removidos, desabilitados ou falharem.

## Objetivo

Implementar registro, descoberta, ciclo de vida, seleção e estado de capacidades plugáveis.

## Resultado observável

O agente inicia sem sensores opcionais, carrega plugins compatíveis, atualiza seu modelo de capacidades, reage à remoção e seleciona implementações alternativas por operação.

## Requisitos funcionais

1. `CapabilityRegistry` persistente e consultável.
2. `PluginDiscovery` por manifesto local e entry point.
3. validação de schema e versão.
4. `ModuleLifecycleManager`.
5. eventos de disponibilidade no event bus.
6. resolução por operação, não por classe concreta.
7. prioridades e fallback explícitos.
8. atualização do self-model.
9. invalidação de planos dependentes.
10. hot-plug quando suportado.
11. profiles de execução com subconjuntos de capacidades.
12. checkpoints de plugins com estado.

## Requisitos não funcionais

- núcleo sem imports de plugins concretos;
- falha isolada;
- inicialização determinística;
- logs estruturados;
- contratos versionados;
- overhead mínimo sem módulos ativos.

## Entradas

- manifests;
- configuração;
- eventos de health check;
- comandos de ativação e remoção.

## Saídas

- capability states;
- eventos de lifecycle;
- seleção de implementação;
- mudanças no self-model;
- erros tipados.

## Estados e transições

`unknown → discovered → calibrating → available`

De `available`:

- `degraded`;
- `temporarily_unavailable`;
- `disabled`;
- `failed`;
- `removed`;
- `incompatible` após atualização inválida.

## Erros esperados

- manifesto inválido;
- versão incompatível;
- dependência ausente;
- permissão insuficiente;
- health check falho;
- timeout de drain;
- checkpoint indisponível;
- operação sem fornecedor.

## Escopo negativo

- implementar sensores concretos;
- instalar código remoto;
- permitir plugins não validados;
- ocultar mudanças de fonte;
- migrar estado cognitivo central para plugins.

## Critérios de aceite

- [x] O núcleo inicia com zero sensores opcionais.
- [x] Um plugin compatível pode ser adicionado sem modificar o núcleo.
- [x] Um plugin ativo pode ser removido sem corromper timeline ou memória.
- [x] O self-model reflete a mudança em até um ciclo cognitivo.
- [x] Operações sem fornecedor retornam erro tipado e não são inventadas.
- [x] Um fallback compatível pode ser selecionado e a troca fica auditada.
- [x] Falha de plugin não encerra event bus nem memória.
- [x] Eventos antigos permanecem legíveis após desinstalação.
- [x] Testes comprovam ausência de imports concretos no pacote `core`.
- [x] Reinicialização restaura registry e estados persistidos aplicáveis.

## Plano de testes

### Unitários

- validação de descriptor;
- transições;
- resolução de dependência;
- seleção por operação;
- erros.

### Integração

- instalar sensor falso;
- emitir eventos;
- remover durante uso;
- fallback;
- reiniciar;
- recuperar memória.

### Contrato

- versões compatíveis e incompatíveis;
- schema de observação;
- lifecycle.

### Degradação

- zero sensores;
- somente um sensor;
- falha simultânea de dois módulos;
- retorno de capacidade;
- módulo degradado.

### Arquitetura

- teste estático que impede imports `sensors.*`, `tools.*` e `actions.*` em `core`.
