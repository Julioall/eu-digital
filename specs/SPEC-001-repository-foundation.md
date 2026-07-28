# SPEC-001 — Fundação documental e governança do repositório

Status: ready
Fase: 0
Dependências: nenhuma
ADRs aplicáveis: ADR-0001, ADR-0002

## Objetivo

Criar a fundação independente de linguagem para documentação, configuração, governança, validação de SPECs e execução dos fluxos autônomos.

## Requisitos

- estrutura normativa de documentação;
- diretório de SPECs e ADRs;
- configuração do projeto;
- logging e relatórios definidos por contrato;
- validador de frontmatter e critérios;
- comandos de verificação documental;
- CI mínima para documentação e schemas;
- nenhuma decisão prematura sobre implementação de domínio.

## Escopo negativo

- runtime cognitivo;
- sensores;
- memória;
- modelos;
- avatar;
- fundação executável Python ou C++.

A fundação executável híbrida pertence à SPEC-025.

## Critérios de aceite

- [ ] Validador rejeita SPEC sem objetivo, escopo negativo ou critérios.
- [ ] ADRs e contratos são localizáveis.
- [ ] Configurações normativas possuem schema.
- [ ] A árvore do repositório é gerada.
- [ ] Fluxo documental funciona sem dependências de IA.
