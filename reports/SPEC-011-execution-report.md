# Relatório de Execução

SPEC: SPEC-011
Agente: Codex
Data: 2026-07-28
Commit: trabalho local não commitado

## Alterações realizadas

- criada a ADR-0013, que fixa a referência local de metacognição calibrada e
  curiosidade limitada por orçamento;
- criados contratos versionados para hipótese, avaliação, pergunta e resposta;
- implementada referência Python que mantém evidência favorável/contrária,
  alternativas, confiança bruta e calibração por outcomes verificados;
- implementadas propostas estruturadas com ganho informacional, silêncio,
  orçamento móvel, cooldown, correção e supressão de redundância;
- registrados controles `raw_confidence_v0` e `fixed_gain_v0`, ablação,
  hipótese, métricas e condição de falsificação;
- documentados o limite do laboratório Python e a ausência explícita de
  diálogo, LLM, busca externa, mensagens, ações ou promoção C++.

## Arquivos modificados

- `docs/04-adrs/ADR-0013-calibrated-metacognition-and-bounded-curiosity.md`;
- `contracts/schemas/{hypothesis,metacognitive_assessment,curiosity_question,curiosity_response}.schema.json`;
- `docs/03-contracts/METACOGNITION_CURIOSITY_SCHEMA.md`;
- `python/eu_digital_lab/metacognition_curiosity.py`;
- `python/tests/test_metacognition_curiosity.py`;
- SPEC, contratos, documentação operacional, API pública do laboratório e
  questões de governança relacionadas.

## Testes executados

```text
PYTHONPATH=python python3 -m unittest python.tests.test_metacognition_curiosity -v
python3 -m compileall -q python/eu_digital_lab python/tests
ruff check (arquivos da SPEC-011)
mypy (arquivos da SPEC-011)
python3 tools/validate_contracts.py
python3 tools/check_promotions.py
python3 tools/validate_sandbox.py datasets/synthetic/v1
python3 tools/validate_hybrid.py
```

## Resultados

- 11 testes específicos aprovados;
- suíte Python completa: 102 testes aprovados;
- Ruff direcionado, mypy e compilação de bytecode aprovados;
- contratos canônicos, gate de promoção e corpus sandbox aprovados;
- fluxo híbrido completo aprovado, com 8/8 CTest e release sem runtime Python.

## Critérios de aceite

- [x] Toda pergunta referencia uma hipótese e uma avaliação;
- [x] Cada proposta contém estimativa de ganho informacional;
- [x] Correções aplicam cooldown e penalizam o ganho de repetição;
- [x] Confiança suficiente produz decisão e supressão de silêncio.

## Desvios

A entrega é somente uma referência de laboratório Python, local e efêmera.
Não há persistência longitudinal, diálogo, LLM, envio de mensagens, busca
externa, ação autônoma, self-model ou promoção C++ nesta SPEC.

## Riscos e pendências

- Brier, ECE, AUROC e risk-coverage só poderão sustentar conclusões após
  protocolo congelado, outcomes independentes e holdout não reutilizado;
- a utilidade humana das perguntas permanece dependente da questão aberta 10;
- qualquer promoção para C++ exige SPEC-026 e validação independente: a
  equivalência entre implementações não é evidência de validade científica.

## Decisões tomadas

- outcomes confirmados/rejeitados são indexados pela confiança bruta para
  recalibrar, enquanto métricas registram a confiança calibrada avaliada;
- resposta inconclusiva permanece explicitamente sem evidência e não é tratada
  como resultado negativo;
- `information_gain_v1` usa entropia e resolução declarada; o controle
  `fixed_gain_v0` preserva a mesma interface;
- perguntas são objetos auditáveis submetidos a orçamento, cooldown,
  redundância e correção, podendo ser suprimidas antes de qualquer entrega.

## Evidências

- ADR: `docs/04-adrs/ADR-0013-calibrated-metacognition-and-bounded-curiosity.md`;
- contratos: `docs/03-contracts/METACOGNITION_CURIOSITY_SCHEMA.md`;
- implementação: `python/eu_digital_lab/metacognition_curiosity.py`;
- testes: `python/tests/test_metacognition_curiosity.py`;
- protocolo: `specs/SPEC-011-metacognition-curiosity.md`.
