# Relatório de Execução

SPEC: SPEC-024 — Adaptação a ausência e novas capacidades
Agente: Codex
Data: 2026-07-29
Commit: feat: complete SPEC-024 capability adaptation

## Alterações realizadas

O bloqueio foi resolvido com ADR-0023 e contratos de evento, perfil de
observabilidade e onboarding. A referência Python recebe somente mudanças de
capacidade, reduz confiança de hipóteses dependentes, redistribui atenção,
invalida predições, bloqueia planos incompatíveis e exige calibração mínima
antes de uma modalidade influenciar crenças estáveis. Retorno/substituição
preserva o agente e seu histórico.

## Arquivos modificados

- python/eu_digital_lab/capability_adaptation.py
- python/tests/test_capability_adaptation.py
- python/eu_digital_lab/__init__.py
- contracts/schemas/capability_adaptation_event.schema.json
- contracts/schemas/observability_profile.schema.json
- contracts/schemas/capability_onboarding.schema.json
- docs/03-contracts/CAPABILITY_ADAPTATION_SCHEMA.md
- docs/04-adrs/ADR-0023-capability-adaptation-and-graceful-degradation.md
- docs/02-architecture/COMPONENT_CATALOG.md
- docs/05-governance/OPEN_QUESTIONS.md
- docs/06-operations/DEVELOPMENT_COMMANDS.md
- contracts/README.md
- python/README.md
- specs/SPEC-024-capability-adaptation-and-graceful-degradation.md

## Testes executados

- validação JSON dos três schemas;
- testes unitários da SPEC-024;
- Ruff e mypy nos arquivos novos;
- suíte Python completa;
- build, CTest, instalação e verificação de release sem Python.

## Critérios de aceite

- [x] Remoção de visão reduz confiança visual e torna a limitação explícita.
- [x] Remoção de áudio não invalida predição baseada em sistema.
- [x] Plano que exige atuador removido é bloqueado.
- [x] Modalidade nova fica em calibração antes de entrar na atenção.
- [x] Retorno mantém `agent_id`, geração e histórico.
- [x] Atenção adaptativa supera a atenção fixa no cenário de ablação.

## Desvios e limites

Nenhum desvio funcional. O módulo não captura dados, executa planos, importa
plugins concretos nem transforma ausência em observação negativa. A métrica de
ablação é evidência operacional do mecanismo; validade ecológica permanece
posterior.

## Evidências

- ADR: `docs/04-adrs/ADR-0023-capability-adaptation-and-graceful-degradation.md`;
- contratos: `contracts/schemas/capability_adaptation_event.schema.json`,
  `observability_profile.schema.json` e `capability_onboarding.schema.json`;
- testes: `python/tests/test_capability_adaptation.py`.
