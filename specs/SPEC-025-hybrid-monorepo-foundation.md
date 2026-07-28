# SPEC-025 — Fundação do monorepositório Laboratório/Cérebro

Status: ready
Fase: 0
Dependências: SPEC-001
ADRs aplicáveis: ADR-0001, ADR-0009, ADR-0010

## Objetivo

Transformar a fundação do projeto em um monorepositório com Laboratório Python, Cérebro Implantado C++ e contratos compartilhados.

## Entregáveis

- diretório `cpp/`;
- diretório `python/`;
- diretório `contracts/`;
- diretório `validation/`;
- CMake Presets;
- `pyproject.toml`;
- comandos unificados;
- build mínimo C++;
- pacote mínimo Python;
- fixture compartilhada;
- teste de leitura da fixture nas duas linguagens;
- CI para ambos os ambientes.

## Requisitos

1. Python não pode ser dependência de runtime do binário C++.
2. Contratos não podem estar duplicados por linguagem.
3. O build C++ deve funcionar sem instalar dependências Python de pesquisa.
4. O laboratório deve funcionar sem compilar o runtime completo quando usar apenas replays.
5. Deve existir comando único para validação completa.
6. O repositório deve distinguir artefatos gerados de fontes normativas.

## Critérios de aceite

- [ ] `cmake --preset dev` configura o Cérebro Implantado.
- [ ] `cmake --build --preset dev` produz executável mínimo.
- [ ] o pacote Python instala em ambiente isolado.
- [ ] Python e C++ leem a mesma fixture de `CanonicalEvent`.
- [ ] alteração incompatível em contrato falha em CI.
- [ ] pacote de release não contém Python.
- [ ] documentação identifica claramente laboratório e runtime.
