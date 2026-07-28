# Relatório de Execução

SPEC: SPEC-017
Agente: Codex
Data: 2026-07-28
Commit: trabalho em `spec/017-research-sandbox`, ainda não commitado

## Alterações realizadas

- criado gerador determinístico de rotinas sintéticas com seed explícita;
- criado modelo de eventos, episódios e links de causalidade com ground truth;
- criada ferramenta CLI para anotação humana de episódios;
- criada ferramenta de concordância interanotadores com acordo exato e Cohen's kappa;
- adicionados schemas versionados para sessões, anotações, concordância e manifesto;
- criado corpus `datasets/synthetic/v1` com treino, desenvolvimento e holdout;
- criado validador de hashes, splits, referências e ausência de dependência LLM;
- ajustado o validador documental para headings CRLF no Windows;
- adicionados testes unitários de reprodutibilidade, truth, splits, anotações e concordância;
- atualizada a documentação operacional e do laboratório Python.

## Arquivos modificados

- `python/eu_digital_lab/`;
- `python/tests/test_sandbox.py`;
- `tools/generate_sandbox_corpus.py`;
- `tools/annotate_session.py`;
- `tools/calculate_agreement.py`;
- `tools/validate_sandbox.py`;
- `schemas/sandbox_session.schema.json`;
- `schemas/annotation.schema.json`;
- `schemas/agreement.schema.json`;
- `schemas/corpus_manifest.schema.json`;
- `datasets/synthetic/v1/` e seus manifestos;
- `datasets/synthetic/README.md`;
- `datasets/annotated/README.md`;
- `README.md`, `python/README.md` e `docs/06-operations/DEVELOPMENT_COMMANDS.md`;
- este relatório.

## Testes executados

```powershell
python -m unittest discover -s python/tests -v
python tools/generate_sandbox_corpus.py --output datasets/synthetic/v1 --sessions 6 --seed 20260728 --split-seed 1701
python tools/validate_sandbox.py datasets/synthetic/v1
```

## Resultados

- 8/8 testes unitários aprovados;
- 6/6 sessões do corpus validadas;
- splits treino, desenvolvimento e teste disjuntos;
- hashes do manifesto conferem;
- ground truth de eventos, episódios e causalidade fechado;
- anotação humana estruturada sem LLM.
- concordância calculável sobre episódios compartilhados.

## Critérios de aceite

- [x] Uma sessão é reproduzível por seed.
- [x] Ground truth possui schema versionado.
- [x] Concordância entre anotadores é calculada.
- [x] Corpus não depende do LLM.

## Desvios

Nenhum desvio funcional. A ferramenta de anotação é CLI deliberadamente; uma
interface gráfica pertence a SPEC posterior e não é necessária para o contrato
de anotação humana.

## Riscos e pendências

- a concordância depende de pelo menos dois arquivos de anotação para a mesma
  sessão;
- o corpus é sintético e não demonstra validade ecológica.

## Decisões tomadas

- usar `random.Random(seed)` e UUIDs derivados por UUID5 para estabilidade;
- manter o holdout `test` registrado no manifesto e fora do desenvolvimento;
- representar agência como ground truth observável, sem inferência cognitiva;
- não importar bibliotecas externas nem LLM.

## Rollback

Reverter o commit da SPEC-017 remove o gerador, schemas, corpus e testes sem
migração de dados de produção. O merge de SPEC-001 continua sendo o baseline.

## Evidências

- dependência satisfeita: SPEC-001 em `abe44ad`;
- branch: `spec/017-research-sandbox`;
- corpus: `datasets/synthetic/v1/manifest.json`;
- testes: 7/7;
- sessões: 6/6.
