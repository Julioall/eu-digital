---
id: SPEC-025
title: Fundação do monorepositório Laboratório/Cérebro
status: done
phase: 0
dependencies: [SPEC-001]
adrs: [ADR-0001, ADR-0009, ADR-0010]
contracts: []
---

# SPEC-025 — Fundação do monorepositório Laboratório/Cérebro

Status: done
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

## Escopo negativo

- runtime cognitivo além do executável mínimo de verificação;
- sensores, memória, modelos e avatar;
- runtime de capacidades da SPEC-023;
- pipeline de promoção da SPEC-026;
- gates científicos e ecológicos da SPEC-027;
- instalação de componentes Python no produto C++ distribuído.

## Critérios de aceite

- [x] `cmake --preset dev` configura o Cérebro Implantado.
- [x] `cmake --build --preset dev` produz executável mínimo.
- [x] o pacote Python instala em ambiente isolado.
- [x] Python e C++ leem a mesma fixture de `CanonicalEvent`.
- [x] alteração incompatível em contrato falha em CI.
- [x] pacote de release não contém Python.
- [x] documentação identifica claramente laboratório e runtime.
