---
id: SPEC-013
title: Gateway de modelo local
status: done
phase: 5
dependencies: [SPEC-001]
adrs: [ADR-0001, ADR-0003, ADR-0004, ADR-0009, ADR-0010, ADR-0011, ADR-0015]
contracts: [model_prompt_template.schema.json, local_model_request.schema.json, local_model_response.schema.json]
---

# SPEC-013 — Gateway de modelo local

Status: concluída
Fase: 5
Dependências: SPEC-001
ADRs: ADR-0001, ADR-0003, ADR-0004, ADR-0009, ADR-0010, ADR-0011, ADR-0015
Contratos: `model_prompt_template.schema.json`, `local_model_request.schema.json`,
`local_model_response.schema.json`

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

Também não inclui escolha, download ou empacotamento de modelo/runtime
concreto, diálogo autônomo, ação, persistência de prompts ou promoção para C++.

## Protocolo operacional

- **Baseline:** FIFO sem prioridade (`fifo_single_worker_v0`);
- **Tratamento:** fila de prioridade estável (`priority_single_worker_v1`) com
  um worker pesado;
- **Métricas:** máximo de inferências e modelos carregados simultaneamente,
  espera por prioridade, timeout, cancelamento, rejeição de saída e
  descarregamento;
- **Ablação:** selecionar FIFO pela mesma interface;
- **Falsificação:** duas inferências/modelos pesados coexistem, timeout não
  descarrega recurso, ou uma resposta inválida é devolvida;
- **Limite:** o gateway é infraestrutura local; não mede inteligência nem
  autoriza um backend concreto.

## Critérios de aceite
- [x] Nunca executa dois modelos pesados simultaneamente.
- [x] Timeout libera recursos.
- [x] Saída inválida é rejeitada.
- [x] Backend pode ser trocado por configuração.

## Plano de testes

- duas requisições simultâneas nunca ocupam o backend pesado ao mesmo tempo;
- prioridade e FIFO são selecionáveis pela mesma configuração;
- timeout e cancelamento chamam o backend e descarregam o modelo;
- template registra versão e rejeita variáveis ausentes/extras;
- resposta fora de `output.kind`/`output.fields` é rejeitada;
- backend é trocado por configuração sem alterar o gateway;
- replay da mesma sequência é determinístico e o módulo não importa LLM/API.
