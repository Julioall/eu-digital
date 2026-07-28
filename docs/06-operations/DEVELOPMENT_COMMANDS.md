# Comandos de Desenvolvimento

Os comandos serão ativados na SPEC de fundação.

Alvo:

```bash
uv sync
uv run ruff check .
uv run ruff format --check .
uv run mypy src
uv run pytest -q
uv run python -m tools.validate_specs
uv run python -m tools.validate_contracts
```

Nenhuma tarefa deve ser marcada como concluída sem executar a sequência aplicável.
