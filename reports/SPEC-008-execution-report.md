# Relatório de Execução

SPEC: SPEC-008
Agente: Codex
Data: 2026-07-28
Commit: trabalho local não commitado

## Alterações realizadas

- implementada memória episódica local no laboratório Python;
- adicionada persistência JSON versionada e idempotência por `episode_id`;
- implementadas consultas por sessão, aplicação, documento, modalidade e
  intervalo temporal;
- adicionados embeddings locais opcionais e relações de similaridade
  explicáveis;
- adicionados códigos de motivo, explicação de recuperação e proveniência dos
  eventos de origem;
- implementada política de consolidação com retenção limitada;
- preservadas hipóteses e fatos observados em campos separados, sem resumo ou
  generalização semântica automática;
- adicionados testes determinísticos de recuperação, persistência, retenção,
  similaridade e separação de hipóteses.

## Arquivos modificados

- `python/eu_digital_lab/episodic_memory.py`;
- `python/eu_digital_lab/__init__.py`;
- `python/tests/test_episodic_memory.py`;
- `specs/SPEC-008-episodic-memory.md`;
- `python/README.md`;
- `docs/06-operations/DEVELOPMENT_COMMANDS.md`.

## Testes executados

```text
PYTHONPATH=python python3 -m unittest python.tests.test_episodic_memory -v
PYTHONPATH=python python3 -m unittest discover -s python/tests -q
PYTHONPATH=python python3 tools/validate_contracts.py
PYTHONPATH=python python3 tools/validate_sandbox.py datasets/synthetic/v1
PYTHONPATH=python python3 tools/validate_hybrid.py --skip-build
/tmp/tmp.HyT0IpIN6S/pkg/ruff-0.16.0.data/scripts/ruff check python/eu_digital_lab/episodic_memory.py python/tests/test_episodic_memory.py
PYTHONPATH=/tmp/tmp.21tTxQm2JN/extracted/usr/lib/python3/dist-packages:python /tmp/tmp.21tTxQm2JN/extracted/usr/bin/mypy python/eu_digital_lab
```

## Resultados

- testes específicos: 6 aprovados;
- suíte Python completa: 73 testes aprovados;
- contratos, sandbox e validador híbrido aprovados;
- Ruff direcionado e mypy aprovados.

## Critérios de aceite

- [x] recupera episódios por contexto;
- [x] explica por que um episódio foi recuperado;
- [x] não mistura hipótese com fato.

## Desvios

Esta é uma implementação de referência Python. Não há promoção automática ao
C++ nem uso de modelo semântico; o provedor de embedding é uma porta opcional
que deve ser local e validada separadamente.

## Riscos e pendências

- o holdout anotado e as métricas Recall@k/MRR ainda precisam de avaliação
  científica fora dos testes unitários;
- a consolidação implementa retenção limitada, mas esquecimento calibrado e
  reconciliação permanecem SPECs posteriores;
- o formato JSON local deve ser substituído ou acompanhado pelo backend de
  produção aprovado quando houver evidência de escala.

## Decisões tomadas

- manter o episódio imutável depois de armazenado;
- usar sinais explícitos de contexto e similaridade, nunca texto gerado como
  único identificador;
- retornar a proveniência em toda recuperação;
- manter hipóteses no campo contratual sem promovê-las a fatos;
- permitir ablação removendo o provedor de embedding e as relações de
  similaridade.

## Evidências

- implementação: `python/eu_digital_lab/episodic_memory.py`;
- testes: `python/tests/test_episodic_memory.py`;
- contrato consumido: `contracts/schemas/episode.schema.json`;
- SPEC/protocolo: `specs/SPEC-008-episodic-memory.md`.
