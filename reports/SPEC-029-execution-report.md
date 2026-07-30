# Relatório de Execução

SPEC: SPEC-029 — Maturidade de componentes e registro de release
Agente: Codex
Data: 2026-07-29
Commit: pendente de criação

## Alterações realizadas

- Criado contrato versionado para separar maturidade da referência,
  implementação nativa e disponibilidade do produto.
- Criado registro com 21 componentes existentes, mantendo mecanismos
  cognitivos sem promoção C++ e sem disponibilidade no produto.
- Criadas validações para IDs duplicados, estados incompatíveis, SPECs não
  resolvíveis e evidências ausentes ou fora da raiz do repositório.
- Criadas fixture válida, fixture inválida e testes de regras de governança.
- Atualizada a documentação operacional, ADR, contrato, SPEC e árvore do
  repositório.

## Arquivos modificados

- `contracts/schemas/component_maturity.schema.json`
- `contracts/fixtures/component_maturity.json`
- `contracts/fixtures/component_maturity.invalid.json`
- `python/eu_digital_lab/component_maturity.py`
- `python/tests/test_component_maturity.py`
- `tools/validate_component_maturity.py`
- `docs/03-contracts/COMPONENT_MATURITY_CONTRACT.md`
- `docs/04-adrs/ADR-0025-component-maturity-and-release-status.md`
- `specs/SPEC-029-component-maturity-and-release-manifest.md`
- `REPOSITORY_TREE.txt`

## Testes executados

- `python tools/validate_component_maturity.py` — 21 componentes válidos.
- `python -m unittest python.tests.test_component_maturity -v` — 5/5.
- `python tools/validate_hybrid.py` — 166 testes Python e 13 CTest.
- `cmake --install build/dev --prefix build/release` — aprovado.
- Validação documental, configuração e árvore — aprovadas.

## Resultados

O registro não interpreta `done` documental como disponibilidade. O runtime
continua apenas candidato nativo e experimental; capacidades cognitivas têm
referência Python, mas `native_status: none` e `product_status: unavailable`.
Nenhum `promotion_id` foi criado ou inferido.

## Critérios de aceite

Todos os sete critérios da SPEC-029 foram atendidos. A SPEC foi marcada como
`done`; isso não altera a disponibilidade dos componentes registrados.

## Desvios

Nenhuma promoção cognitiva, integração de sensor, modelo, interface, avatar,
ação ou empacotamento de produto foi antecipada.

## Riscos e pendências

- O smoke test nativo Windows da SPEC-028 ainda precisa ser executado em um
  Developer PowerShell nativo. Essa pendência não invalida o contrato da
  SPEC-029, mas mantém o Runtime Preview sem gate Windows fechado.
- Os estados de produto continuam conservadores até as SPECs de privacidade,
  observação, promoção e distribuição serem aprovadas.

## Decisões tomadas

- `evidence_refs` são caminhos locais e devem existir no momento da validação.
- A maturidade é registrada por componente, sem substituir o registro de
  promoções ou os estados documentais das SPECs.

## Evidências

- `valid component maturity registry: 21 components`.
- `Ran 5 tests ... OK` para a suíte específica.
- `hybrid validation passed; release contains no Python runtime files`.
