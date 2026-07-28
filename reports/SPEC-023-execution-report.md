# Relatório de Execução

SPEC: SPEC-023  
Agente: Codex  
Data: 2026-07-28  
Commit: trabalho local não commitado

## Alterações realizadas

- implementado o runtime Python de capacidades removíveis;
- adicionados `CapabilityDescriptor`, `CapabilityState`, `PluginManifest`,
  discovery por manifesto local e entry point;
- implementados lifecycle, dependências obrigatórias, permissões, perfis,
  resolução por operação, fallback auditado, checkpoints e persistência;
- implementada atualização do self-model e invalidação de planos dependentes;
- adicionados schemas executáveis compartilhados para descriptor, state,
  observation envelope, self-model e plugin manifest;
- implementada a porta C++ nativa de registry, resolver e lifecycle;
- adicionados testes Python e CTest para ausência, falha, remoção,
  reinstalação, substituição e persistência;
- atualizadas documentações de contratos, operações, Python, C++ e questões
  abertas.

## Arquivos modificados

- `python/eu_digital_lab/capabilities.py`;
- `python/eu_digital_lab/schema_validation.py`;
- `python/eu_digital_lab/__init__.py`;
- `python/tests/test_capabilities.py`;
- `contracts/schemas/capability_descriptor.schema.json`;
- `contracts/schemas/capability_state.schema.json`;
- `contracts/schemas/observation_envelope.schema.json`;
- `contracts/schemas/self_model.schema.json`;
- `contracts/schemas/plugin_manifest.schema.json`;
- `cpp/core/capability_runtime.hpp`;
- `cpp/tests/capability_runtime_test.cpp`;
- `CMakeLists.txt`;
- documentação em `contracts/`, `cpp/`, `python/`, `docs/03-contracts/`,
  `docs/05-governance/` e `docs/06-operations/`.

## Testes executados

```text
PYTHONPATH=python python3 -m unittest discover -s python/tests -v
python3 -m compileall -q python
JSON parse dos schemas em contracts/schemas/
PYTHONPATH=python python3 tools/validate_sandbox.py datasets/synthetic/v1
ruff check python/eu_digital_lab/capabilities.py python/eu_digital_lab/schema_validation.py python/tests/test_capabilities.py
mypy --python-executable /usr/bin/python3 python/eu_digital_lab
cmake -S . -B build/spec023-isolated7 -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build/spec023-isolated7
ctest --test-dir build/spec023-isolated7 --output-on-failure
```

## Resultados

- 34 testes Python aprovados;
- módulos Python compilados com sucesso;
- schemas JSON parseados com sucesso;
- corpus sintético validado: 6 sessões e 6 arquivos;
- lint direcionado aprovado;
- mypy aprovado em 7 módulos Python;
- CMake configurado e build C++ aprovado;
- CTest: 3/3 testes aprovados;
- teste arquitetural não encontrou imports concretos no core Python/C++.

## Critérios de aceite

- [x] núcleo Python inicia com zero capacidades opcionais;
- [x] plugin compatível é descoberto por manifesto sem alterar o core;
- [x] entry points são descobertos;
- [x] descriptors inválidos e versões/estados incompatíveis são rejeitados;
- [x] lifecycle e falha isolada são registrados no event bus;
- [x] resolução ocorre por operação com prioridade e fallback auditado;
- [x] ausência de fornecedor retorna erro tipado;
- [x] remoção preserva histórico e invalida planos dependentes;
- [x] self-model reflete alterações de capacidade;
- [x] reinstalação exige nova validação;
- [x] registry, estados e checkpoints são restaurados após reinício;
- [x] C++ possui implementação e teste dedicado;
- [x] compilação e CTest do runtime C++.

## Desvios

O runtime Python foi implementado como referência de laboratório e a porta C++
como runtime nativo abstrato. Discovery de manifestos e entry points permanece
no laboratório Python; o C++ recebe descritores e plugins nativos por interfaces
abstratas, sem dependência de Python.

Durante a validação foi corrigida apenas a formatação de fim de linha dos seis
JSONs do corpus sintético; os hashes versionados voltaram a coincidir sem
alteração semântica. CMake, Ninja, GCC, mypy e Ruff foram carregados em
diretórios temporários, sem instalação sistêmica.

## Riscos e pendências

- o lint de todo o repositório ainda encontra problemas preexistentes fora do
  escopo da SPEC-023;
- equivalência Python/C++ de mecanismos promovidos permanece regida pela
  SPEC-026, fora do escopo desta SPEC.

## Decisões tomadas

- manifestos locais e entry points Python são os mecanismos iniciais de
  discovery, conforme os requisitos explícitos da SPEC-023;
- hot-plug é aplicado quando declarado pelo descriptor;
- operações não possuem catálogo fixo: o descriptor é a fonte de capacidades;
- a implementação Python usa somente biblioteca padrão;
- schemas executáveis vivem em `contracts/schemas/`.

## Evidências

- SPEC: `specs/SPEC-023-pluggable-capability-runtime.md`;
- contratos: `docs/03-contracts/` e `contracts/schemas/`;
- testes: `python/tests/test_capabilities.py`;
- runtime C++: `cpp/core/capability_runtime.hpp`;
- alvo CTest: `capability_runtime` em `CMakeLists.txt`.
