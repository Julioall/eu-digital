# Comandos de Desenvolvimento

## Fundação documental

Ativados pela SPEC-001 e executáveis no Windows PowerShell sem dependências
externas:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tests/documentation/Test-Documentation.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File tools/validate_documentation.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File tools/generate_repository_tree.ps1
```

O último comando atualiza `REPOSITORY_TREE.txt`. A CI executa os testes e o
validador em modo somente leitura; uma árvore desatualizada falha.

## Sandbox científico

```powershell
python -m unittest discover -s python/tests -v
python tools/validate_contracts.py
python tools/generate_sandbox_corpus.py --output datasets/synthetic/v1
python tools/validate_sandbox.py datasets/synthetic/v1
# após duas anotações humanas da mesma sessão:
python tools/calculate_agreement.py anotador-1.json anotador-2.json agreement.json
```

O corpus sintético é determinístico, versionado por manifesto e não importa
LLM ou serviços externos. O split `test` é o holdout bloqueado.

## Fundação executável

Os comandos abaixo são alvos da SPEC-025 e ainda não fazem parte da
SPEC-001:

```bash
uv sync
uv run ruff check .
uv run ruff format --check .
uv run mypy src
uv run pytest -q
```

O comando unificado da SPEC-025 é:

```powershell
uv run python tools/validate_hybrid.py
```

Ele executa testes Python, configura/compila/testa o C++ e instala uma release
mínima, verificando que nenhum arquivo Python é empacotado.

Nenhuma tarefa deve ser marcada como concluída sem executar a sequência aplicável.
