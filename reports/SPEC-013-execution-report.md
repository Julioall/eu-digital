# Relatório de Execução

SPEC: SPEC-013
Agente: Codex
Data: 2026-07-29
Commit: trabalho local não commitado

## Alterações realizadas

- criada ADR-0015 para um gateway local por porta injetada e worker pesado
  único, sem escolher modelo, runtime concreto ou API;
- criados contratos versionados de template, requisição e resposta estruturada;
- implementado gateway Python com fila estável de prioridade ou FIFO, um worker
  de inferência, carregamento, cancelamento, timeout e descarregamento;
- implementada troca de backend pela configuração quando o gateway está ocioso;
- validada saída exclusivamente como `output.kind` e `output.fields`, rejeitando
  qualquer formato cru inválido;
- instalados localmente no perfil do usuário Ruff, mypy, CMake, Ninja e um
  toolchain temporário GCC 13 para restaurar a validação híbrida deste ambiente.

## Arquivos modificados

- `docs/04-adrs/ADR-0015-local-model-gateway-port-and-single-heavy-worker.md`;
- `contracts/schemas/{model_prompt_template,local_model_request,local_model_response}.schema.json`;
- `docs/03-contracts/LOCAL_MODEL_GATEWAY_SCHEMA.md`;
- `python/eu_digital_lab/local_model_gateway.py`;
- `python/tests/test_local_model_gateway.py`;
- SPEC, contratos, documentação operacional, API pública do laboratório e
  governança relacionadas.

## Testes executados

```text
PYTHONPATH=python python3 -m unittest python.tests.test_local_model_gateway -v
python3 -m compileall -q python/eu_digital_lab python/tests
ruff check (arquivos da SPEC-013)
mypy (arquivos da SPEC-013)
python3 tools/validate_contracts.py
python3 tools/check_promotions.py
python3 tools/validate_sandbox.py datasets/synthetic/v1
python3 tools/validate_hybrid.py
```

## Resultados

- 9 testes específicos aprovados;
- suíte Python completa: 120 testes aprovados;
- Ruff, mypy, compilação de bytecode e validadores de contrato/promoção/sandbox
  aprovados;
- fluxo híbrido aprovado, com 8/8 CTest e release sem runtime Python.

## Critérios de aceite

- [x] worker único mantém máximo de uma inferência/modelo pesado local;
- [x] timeout solicita cancelamento e descarrega o recurso;
- [x] saída fora de `kind`/`fields` é rejeitada por erro tipado;
- [x] backend é substituído por `GatewayConfig` sem alterar a interface.

## Desvios

Não foi escolhido, baixado ou empacotado nenhum modelo multimodal ou runtime
concreto. O backend usado nos testes é determinístico e injetado. O gateway
não envia dados à rede, não produz diálogo autônomo, não executa ação e não é
promovido para C++ nesta SPEC.

## Riscos e pendências

- a escolha do modelo multimodal local continua a questão aberta 5 e exige ADR
  específica, avaliação de hardware e contrato de backend concreto;
- métricas de fila, timeout e descarregamento são operacionais, não evidência
  de qualidade, cognição ou segurança de um modelo futuro;
- promoção C++ depende da SPEC-026 e validação independente.

## Decisões tomadas

- a porta `LocalModelBackend` impede dependência estrutural de runtime/modelo;
- prioridade é estável por sequência de chegada; FIFO permanece controle pela
  mesma interface;
- cada timeout percorre cancelamento e descarregamento no bloco de liberação;
- templates exigem conjunto exato de variáveis e a saída não é convertida em
  texto livre quando o contrato falha.

## Evidências

- ADR: `docs/04-adrs/ADR-0015-local-model-gateway-port-and-single-heavy-worker.md`;
- contratos: `docs/03-contracts/LOCAL_MODEL_GATEWAY_SCHEMA.md`;
- implementação: `python/eu_digital_lab/local_model_gateway.py`;
- testes: `python/tests/test_local_model_gateway.py`;
- protocolo: `specs/SPEC-013-local-model-gateway.md`.
