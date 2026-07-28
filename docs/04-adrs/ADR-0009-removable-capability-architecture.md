# ADR-0009 — Arquitetura de capacidades removíveis

Status: aceito  
Data: 2026-07-28

## Contexto

O agente deve aprender e operar com diferentes combinações de sensores e ferramentas. Acoplamento direto criaria um “cérebro” dependente de uma configuração fixa, impediria experimentos de ablação e tornaria a perda de uma modalidade uma falha sistêmica.

## Decisão

Adotar arquitetura de portas e adaptadores com:

- registro dinâmico de capacidades;
- plugins versionados;
- eventos canônicos;
- ausência como estado explícito;
- seleção por operação;
- hot-plug;
- degradação graciosa;
- atualização causal do self-model;
- compatibilidade retroativa dos registros de memória.

O núcleo cognitivo não importará implementações concretas de sensores, ferramentas ou atuadores.

## Consequências positivas

- sensores podem ser removidos e substituídos;
- novas modalidades podem ser adicionadas;
- ablações tornam-se reproduzíveis;
- o agente consegue representar limitações;
- memória não depende do plugin original;
- hardware e modelos podem evoluir por partes.

## Custos

- contratos e manifests mais rigorosos;
- necessidade de registry, lifecycle manager e compatibility checks;
- maior número de estados de falha;
- testes combinatórios;
- necessidade de modelar observabilidade parcial.

## Rejeitado

- lista fixa de sensores compilada no núcleo;
- acesso direto do LLM a ferramentas concretas;
- fallback silencioso;
- recriar identidade após mudança de hardware.
