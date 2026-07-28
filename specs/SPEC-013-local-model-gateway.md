# SPEC-013 — Gateway de modelo local

Status: blocked
Fase: 5
Dependências: SPEC-001
ADRs: ADR-0003, ADR-0004

## Objetivo
Carregar e invocar um modelo multimodal local sob demanda, respeitando um modelo pesado por vez.

## Requisitos
- Backend substituível.
- Fila de prioridade.
- Timeout e cancelamento.
- Descarregamento.
- Templates de prompt versionados.
- Respostas estruturadas validadas.

## Escopo negativo
Treino do LLM e dependência de API.

## Critérios de aceite
- [ ] Nunca executa dois modelos pesados simultaneamente.
- [ ] Timeout libera recursos.
- [ ] Saída inválida é rejeitada.
- [ ] Backend pode ser trocado por configuração.
