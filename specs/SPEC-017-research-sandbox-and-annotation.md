# SPEC-017 — Sandbox e corpus de avaliação

Status: ready
Fase: 0.5
Dependências: SPEC-001
ADRs: ADR-0005, ADR-0008

## Objetivo
Criar ambiente reprodutível e corpus anotado para testar sensores, episódios, padrões e agência.

## Requisitos
- gerador de rotinas sintéticas;
- ground truth de eventos, episódios e causalidade;
- ferramenta de anotação humana;
- separação treino, desenvolvimento e teste;
- versionamento de corpus.

## Escopo negativo
Inferência cognitiva de produção.

## Critérios de aceite
- [ ] Uma sessão é reproduzível por seed.
- [ ] Ground truth possui schema versionado.
- [ ] Concordância entre anotadores é calculada.
- [ ] Corpus não depende do LLM.
