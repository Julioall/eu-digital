# Eu Digital — Sistema Operacional Documental

Repositório de especificação para construção assistida por agentes de uma IA local, multimodal e progressivamente autônoma.

## Objetivo central

Construir um agente digital local que:

1. observe o ambiente computacional e, futuramente, físico;
2. transforme sinais em eventos e episódios;
3. forme memórias e modelos de padrões;
4. mantenha um modelo funcional de si, do usuário e do ambiente;
5. formule perguntas, previsões e sugestões;
6. evolua gradualmente para ações supervisionadas;
7. permaneça orientado por experiência, e não por tarefas previamente cadastradas.

O sistema não declara nem presume consciência fenomenal. O objetivo técnico é implementar mecanismos cognitivos funcionais: percepção, memória, atenção, integração, metacognição, curiosidade, aprendizagem temporal e identidade operacional.

## Ordem obrigatória de leitura por agentes

1. `AGENTS.md`
2. `docs/00-governance/CONSTITUTION.md`
3. `docs/00-governance/GLOSSARY.md`
4. `docs/01-product/PRODUCT_VISION.md`
5. `docs/02-architecture/REFERENCE_ARCHITECTURE.md`
6. `docs/02-architecture/COGNITIVE_MODEL.md`
7. `docs/03-contracts/`
8. a SPEC atribuída em `specs/`
9. o plano correspondente em `plans/`
10. os ADRs aplicáveis em `docs/04-adrs/`

## Regra de execução

Nenhum agente deve implementar uma funcionalidade sem:

- SPEC aprovada;
- dependências satisfeitas;
- critérios de aceite mensuráveis;
- testes definidos antes da implementação;
- escopo negativo explícito;
- contratos de dados identificados;
- registro das decisões arquiteturais relevantes.

## Status

Este pacote contém a fundação documental e as primeiras SPECs. O código de produção deve ser criado incrementalmente a partir delas.


## Validação científica

A pasta `docs/07-research/` contém o relatório de validação, matriz de evidências, comparações com arquiteturas existentes, programa experimental, limitações e bibliografia.

Nenhuma alegação de self funcional deve ser feita antes dos gates definidos no programa experimental.

## Arquitetura de corpo variável

Sensores e ferramentas são plugins removíveis. O núcleo cognitivo depende somente de capacidades abstratas e eventos canônicos. Consulte:

- `docs/02-architecture/PLUGGABLE_CAPABILITY_ARCHITECTURE.md`
- `docs/04-adrs/ADR-0009-removable-capability-architecture.md`
- `specs/SPEC-023-pluggable-capability-runtime.md`
- `specs/SPEC-024-capability-adaptation-and-graceful-degradation.md`

## Arquitetura definitiva: Laboratório e Cérebro Implantado

O projeto adota dois ambientes no mesmo monorepositório:

- **Laboratório Python:** hipóteses, protótipos, treinamento, análise e implementações de referência.
- **Cérebro Implantado C++:** runtime local permanente, plugins nativos, inferência, interface e instalador.

O produto distribuído não depende de Python.

Fluxo obrigatório:

```text
Python → validação → contrato → C++ → equivalência → benchmark → instalação
```

Documentos centrais:

- `docs/02-architecture/LAB_AND_DEPLOYED_BRAIN_ARCHITECTURE.md`
- `docs/04-adrs/ADR-0010-python-laboratory-cpp-deployed-brain.md`
- `specs/SPEC-025-hybrid-monorepo-foundation.md`
- `specs/SPEC-026-python-to-cpp-promotion-pipeline.md`

Versão arquitetural: 4 — Laboratório Python e Cérebro Implantado C++.

## Validação científica final

A arquitetura recebeu parecer **aprovado com condições obrigatórias**.

A correção metodológica central é:

```text
equivalência Python–C++ ≠ validade científica
```

O projeto exige verificação, ground truth, holdout, testes metamórficos, equivalência cruzada, sessões online e avaliação longitudinal.

Consulte `docs/07-research/FINAL_SCIENTIFIC_VALIDATION_2026.md`.
