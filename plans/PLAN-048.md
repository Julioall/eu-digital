# Plano de Execução: SPEC-048 — Structured Cognitive Output and Dialogue

Status: concluído em 2026-08-04

## Pré-condições verificadas

- SPEC-045, SPEC-040 e SPEC-042 concluídas;
- ADR-0015 e ADR-0016 preservadas;
- ADR-0035 aceita após aprovação humana dos DTOs versionados;
- `CognitiveCycleResult` 1.0 congelado e sem texto/UI;
- nenhum backend, modelo, API ou serviço externo selecionado.

## Contratos congelados

- `COGNITIVE_OUTPUT_CONTRACTS.md`;
- `cognitive_output_request.schema.json`;
- `language_rendering_candidate.schema.json`;
- `cognitive_output.schema.json`.

## Incrementos executados

1. Publicar os contratos e fixtures 1.0 estritos.
2. Corrigir a decisão explícita para não invocar nem mutar o orçamento proativo.
3. Substituir o renderer preliminar por parser JSON estrito, timeout cancelável,
   isolamento de trabalho não cooperativo e fallback sem conteúdo factual.
4. Adicionar `CognitiveOutputCoordinator` com fila limitada e resolução de
   capacidades para renderer e apresentação.
5. Publicar requests somente após commit live do ciclo, sem alterar
   `CognitiveCycleResult` e sem efeitos em replay.
6. Registrar no shell Qt um renderer de fallback sem backend e uma apresentação
   enfileirada na thread da UI, sem alterar QML.
7. Cobrir ausência, falha, remoção, reinstalação, substituição, backpressure,
   duplicata, timeout e contratos inválidos.

## Validações

```powershell
cmake --build --preset windows-dev --target all
ctest --test-dir build/windows-dev --output-on-failure
python -m pytest python/tests -q
python -m ruff check python/tests/test_cognitive_output_contracts.py
python -m mypy python/eu_digital_lab
powershell -File tests/documentation/Test-Documentation.ps1
powershell -File tools/validate_specs.ps1
cmake --build --preset windows-qt --target qt_dialogue_presentation_adapter_test eu_digital_desktop desktop_integration_test
ctest --test-dir build/windows-qt -R "^qt_dialogue_presentation_adapter$" --output-on-failure
```

## Rollback

Definir `RuntimeConfig.enable_cognitive_output=false` ou remover as capacidades
`language.render` e `presentation.present`. Nenhuma migração ou exclusão de dado
é necessária.
