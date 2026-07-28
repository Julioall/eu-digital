# Relatório de Execução

SPEC: SPEC-012
Agente: Codex
Data: 2026-07-28
Commit: trabalho local não commitado

## Alterações realizadas

- criada ADR-0014 para preservar o contrato de self-model da SPEC-023 e
  definir snapshots funcionais, imutáveis e causais como contratos paralelos;
- adicionados schemas para eventos internos, snapshots versionados e decisões
  estruturais de orquestração;
- implementada referência Python com histórico imutável encadeado por hash,
  atualização por evento e recuperação de versão;
- separados fatos, hipóteses e configuração em coleções distintas com
  proveniência;
- implementado `self_model_gate_v1`, que consulta o snapshot para explicar e
  permitir ou bloquear uma decisão estrutural de capacidade;
- registrado `unconstrained_decision_v0` como baseline removível para ablação;
- ampliado o validador local para referências JSON Schema em `$defs` usadas
  pelos novos contratos.

## Arquivos modificados

- `docs/04-adrs/ADR-0014-versioned-causal-functional-self-model.md`;
- `contracts/schemas/{self_model_internal_event,functional_self_model_snapshot,self_model_decision}.schema.json`;
- `docs/03-contracts/FUNCTIONAL_SELF_MODEL_SCHEMA.md`;
- `python/eu_digital_lab/functional_self_model.py`;
- `python/eu_digital_lab/schema_validation.py`;
- `python/tests/test_functional_self_model.py`;
- SPEC, documentação operacional, API pública do laboratório e governança
  relacionadas.

## Testes executados

```text
PYTHONPATH=python python3 -m unittest python.tests.test_functional_self_model -v
PYTHONPATH=python python3 -m unittest python.tests.test_capabilities python.tests.test_functional_self_model -v
ruff check (arquivos da SPEC-012)
mypy (arquivos da SPEC-012)
python3 tools/validate_contracts.py
python3 tools/check_promotions.py
python3 tools/validate_sandbox.py datasets/synthetic/v1
python3 tools/validate_hybrid.py
```

## Resultados

- 9 testes específicos aprovados;
- integração com capability runtime: 26 testes aprovados;
- suíte Python completa: 111 testes aprovados;
- Ruff direcionado, mypy e validadores de contratos/promoção/sandbox aprovados;
- fluxo híbrido aprovado, com 8/8 CTest e release sem runtime Python.

## Critérios de aceite

- [x] mudança de capacidade cria nova versão e preserva a anterior;
- [x] decisão explica capacidade disponível, degradada, indisponível, removida
  ou não verificada;
- [x] `self_model_gate_v1` consulta o snapshot atual para decidir;
- [x] `version(n)` recupera snapshots anteriores imutáveis.

## Desvios

Esta é uma referência Python, local e em memória. A decisão é estrutural e não
executa uma ferramenta ou ação. Não há integração de plugin concreto, diálogo,
LLM, persistência longitudinal, promoção C++, personalidade, sentimentos ou
alegação de subjetividade.

## Riscos e pendências

- o histórico não persiste entre processos; essa decisão pertence a uma SPEC
  futura de continuidade, não a esta fase;
- H6 requer protocolo congelado, cenários independentes e holdout para avaliar
  acurácia de capacidade e efeito causal além dos testes de engenharia;
- qualquer ligação com ações deve manter o bloqueio estrutural e passar por
  validação de segurança, SPEC-026 e evidência independente.

## Decisões tomadas

- o schema público `self_model.schema.json` permanece compatível;
- capacidade ausente é `unverified`, não indisponibilidade inferida;
- estados explicitamente declarados recebem reason codes distintos e uma
  explicação auditável;
- o baseline selecionável não consulta o snapshot, tornando a ablação causal
  observável sem executar ação;
- snapshots incluem hash que incorpora a identidade local, a versão, o
  predecessor e o conteúdo canônico da atualização.

## Evidências

- ADR: `docs/04-adrs/ADR-0014-versioned-causal-functional-self-model.md`;
- contratos: `docs/03-contracts/FUNCTIONAL_SELF_MODEL_SCHEMA.md`;
- implementação: `python/eu_digital_lab/functional_self_model.py`;
- testes: `python/tests/test_functional_self_model.py`;
- protocolo: `specs/SPEC-012-self-model.md`.
