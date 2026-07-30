# Relatório de Execução

SPEC: SPEC-031 — Observação Windows de baixo risco
Agente: Codex
Data: 2026-07-29
Commit: pendente de criação

## Alterações realizadas

- Criado contrato versionado `CapturePolicy` com denylist sensível mandatória,
  allowlist, pausa global e versão de redator.
- Alterado o sensor de atividade do sistema para publicar executável e
  categoria, filtrando aplicativos bloqueados antes do evento.
- Título de janela passou a ser desabilitado por padrão; quando habilitado por
  allowlist, o evento recebe somente marcador de comprimento e indicador
  textual.
- Clipboard passou a ser desabilitado por padrão no sensor de input; sua
  habilitação exige policy explícita e o evento não contém bytes do conteúdo.
- Adicionadas pausa global, supressão por aplicativo e health observável.
- Adaptadores Windows não leem título quando a opção está desabilitada.
- Criados referência Python, fixtures, ADR, contrato, testes e documentação.

## Arquivos modificados

- `cpp/core/observation_privacy.hpp`
- `cpp/core/system_activity_sensor.hpp`
- `cpp/core/input_interaction_sensor.hpp`
- `cpp/tests/system_activity_sensor_test.cpp`
- `cpp/tests/input_interaction_sensor_test.cpp`
- `contracts/schemas/capture_policy.schema.json`
- `contracts/fixtures/capture_policy*.json`
- `python/eu_digital_lab/observation_policy.py`
- `python/tests/test_observation_policy.py`
- `tools/validate_observation_policy.py`
- `docs/03-contracts/OBSERVATION_POLICY_CONTRACT.md`
- `docs/04-adrs/ADR-0027-low-risk-windows-observation-policy.md`
- `specs/SPEC-031-windows-low-risk-observation.md`
- `docs/03-contracts/PRIVACY_STORAGE_CONTRACTS.md`
- `contracts/README.md`
- `REPOSITORY_TREE.txt`

## Testes executados

- `python tools/validate_observation_policy.py` — aprovado.
- `python -m unittest python.tests.test_observation_policy -v` — 4/4.
- `python tools/validate_hybrid.py` — 177 testes Python e 14 CTest.
- `ctest --test-dir build/dev --output-on-failure` — 14/14.
- Helper Windows nativo — compilação e 14/14 CTest aprovados.
- Validação documental, SPECs, contratos e árvore — aprovadas.

## Resultados

O caminho default não publica título de janela nem clipboard, bloqueia
aplicativos sensíveis e preserva eventos agregados de atividade. A habilitação
textual requer allowlist; o título é redigido para comprimento e o health
registra supressões e pausa global.

## Critérios de aceite

Todos os critérios da SPEC-031 passaram em Linux/WSL e Windows nativo. A SPEC
pode ser considerada concluída.

## Desvios

OCR, captura visual, texto digitado e integração de sensores no RuntimeHost não
foram implementados; pertencem à SPEC-032 ou às integrações posteriores.

## Riscos e pendências

- A denylist usa correspondência local por nome de executável e deve ser
  revisada antes de alegações de cobertura total de aplicativos sensíveis.
- O processo Windows é observado somente como identificador/categoria; o
  conteúdo de título permanece fora do evento default.

## Decisões tomadas

- O token curto `tor` usa correspondência específica para não bloquear
  falsamente executáveis como `editor.exe`.
- Pausa é estado operacional saudável e não falha de capacidade.
- `CapturePolicy` é separado do contrato de consentimento persistido e será
  consumido por OCR somente após a SPEC-032.

## Evidências

- `valid low-risk observation policy`.
- `Ran 4 tests ... OK` na suíte específica.
- `Ran 177 tests ... OK` e `100% tests passed out of 14` na validação híbrida.
- `Windows Runtime Preview validation passed.`
