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

Nenhuma tarefa deve ser marcada como concluída sem executar a sequência aplicável.
