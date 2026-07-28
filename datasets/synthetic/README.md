# Corpus sintético versionado

`v1/` é gerado por `tools/generate_sandbox_corpus.py` com seed explícita. O
arquivo `manifest.json` registra seeds, splits, hashes e o holdout bloqueado.

Os eventos, episódios e links causais possuem verdade conhecida. O corpus não
importa modelos nem serviços de linguagem; sua finalidade é validar geradores,
segmentação, memória e atribuição de agência em ambiente controlado.

Validação:

```powershell
python tools/validate_sandbox.py datasets/synthetic/v1
```
