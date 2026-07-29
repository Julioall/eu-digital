# Relatório de Execução

SPEC: SPEC-014
Agente: Codex
Data: 2026-07-29
Commit: trabalho local não commitado

## Alterações realizadas

- criada ADR-0016 para uma porta de apresentação de avatar não bloqueante e
  substituível;
- criados contratos versionados de notice contextual, feedback e estado visual;
- implementado controller Python local para perguntas/notificações, histórico,
  orçamento de interrupções e ações `correct`, `defer` e `silence`;
- garantidos por contrato os invariantes de não bloqueio, não captura de input
  e ausência de foco;
- mantida a escolha de framework desktop fora da referência de laboratório.

## Arquivos modificados

- `docs/04-adrs/ADR-0016-nonblocking-avatar-presentation-port.md`;
- `contracts/schemas/{dialogue_notice,dialogue_feedback,avatar_view_state}.schema.json`;
- `docs/03-contracts/DIALOGUE_AVATAR_SCHEMA.md`;
- `python/eu_digital_lab/dialogue_avatar.py`;
- `python/tests/test_dialogue_avatar.py`;
- SPEC, contratos, documentação operacional, API pública do laboratório e
  governança relacionadas.

## Testes executados

```text
PYTHONPATH=python python3 -m unittest python.tests.test_dialogue_avatar -v
python3 -m compileall -q python/eu_digital_lab python/tests
ruff check (arquivos da SPEC-014)
mypy (arquivos da SPEC-014)
python3 tools/validate_contracts.py
python3 tools/check_promotions.py
python3 tools/validate_sandbox.py datasets/synthetic/v1
python3 tools/validate_hybrid.py
```

## Resultados

- 8 testes específicos aprovados;
- suíte Python completa: 128 testes aprovados;
- Ruff, mypy, compilação de bytecode e validadores de contrato/promoção/sandbox
  aprovados;
- fluxo híbrido aprovado, com 8/8 CTest e release sem runtime Python.

## Critérios de aceite

- [x] estado visual não bloqueia o trabalho, captura input ou recebe foco;
- [x] pergunta preserva hipótese, confiança, contexto e motivo;
- [x] usuário pode corrigir, adiar e silenciar, com histórico estruturado.

## Desvios

Não foi escolhido ou incorporado framework desktop. A entrega é um controller
de laboratório com presenter opcional; não abre janelas, não declara emoção ou
consciência, não executa ações e não implementa personalidade.

## Riscos e pendências

- a utilidade de uma superfície visual requer avaliação humana e testes em
  ambiente desktop real;
- a questão aberta 7 continua para escolha futura de Tauri, Qt ou outro host;
- feedback operacional não prova diálogo humano nem validade cognitiva.

## Decisões tomadas

- notices expõem evidência contextual e motivo antes da interação;
- orçamento, silêncio e adiamento impedem interrupções repetitivas;
- o presenter recebe apenas estados que já validam invariantes de não bloqueio;
- a ausência do host não elimina o histórico nem vira observação negativa.

## Evidências

- ADR: `docs/04-adrs/ADR-0016-nonblocking-avatar-presentation-port.md`;
- contratos: `docs/03-contracts/DIALOGUE_AVATAR_SCHEMA.md`;
- implementação: `python/eu_digital_lab/dialogue_avatar.py`;
- testes: `python/tests/test_dialogue_avatar.py`;
- protocolo: `specs/SPEC-014-dialogue-avatar.md`.
