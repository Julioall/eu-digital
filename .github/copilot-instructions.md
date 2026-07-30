Leia `AGENTS.md` antes de qualquer modificação.

Este projeto é orientado por especificações. Implemente somente uma SPEC por vez. A SPEC ativa deve possuir critérios de aceite, dependências e escopo negativo. Não invente requisitos.

O sistema deve funcionar localmente e de forma modular. O LLM é um componente de interpretação e diálogo, não o único núcleo cognitivo. Percepção, memória, atenção, aprendizagem de padrões, metacognição e orquestração devem permanecer desacopladas.

Antes de concluir uma alteração, execute os comandos documentados em `docs/06-operations/DEVELOPMENT_COMMANDS.md` e produza um relatório em `reports/`.

## Dual-track implementation

The repository contains a Python research laboratory and a C++ deployed runtime.

- Do not add Python runtime dependencies to the installed C++ product.
- Do not port experimental code before its hypothesis and reference behavior are frozen.
- Treat `contracts/` as the language-independent source of truth.
- Every promoted cognitive component requires cross-language equivalence tests.

## Scientific validity boundary

- The Python reference is not scientific ground truth.
- Cross-language equivalence is necessary but insufficient.
- Add ground-truth fixtures, metamorphic tests, locked holdouts, and online ecological tests.
- Never infer cognitive validity from latency, memory use, or persuasive text.
