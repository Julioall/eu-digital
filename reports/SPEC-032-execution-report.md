# Relatório de Execução

SPEC: SPEC-032 — OCR consentido como capacidade independente  
Agente: Codex  
Data: 2026-07-29  
Commit: pendente de criação

## Alterações realizadas

- Criada a política versionada `ScreenOcrCapturePolicy`, com default negado,
  modos `explicit` e `on_demand`, ID de solicitação, pausa global, região,
  redator e retenção.
- O sensor C++ passou a exigir consentimento resolvido e autorização antes de
  chamar o `ImageStore` ou o `OcrEngine`.
- Regiões válidas são recortadas antes do armazenamento; regiões fora do frame
  são rejeitadas sem persistência.
- Eventos mantêm somente referência/hash, dimensões e metadados. Texto OCR é
  redigido com `length-only-v1`.
- Falhas do motor preservam o evento visual e emitem
  `screen.ocr_unavailable`, sem inventar ausência de texto.
- Health, pausa, revogação em tempo de execução, substituição de portas e
  hot-plug permanecem observáveis.

## Arquivos modificados

- `cpp/core/screen_ocr_policy.hpp`
- `cpp/core/screen_ocr_sensor.hpp`
- `cpp/tests/screen_ocr_sensor_test.cpp`
- `contracts/schemas/screen_ocr_capture_policy.schema.json`
- `contracts/fixtures/screen_ocr_capture_policy*.json`
- `python/eu_digital_lab/screen_ocr_policy.py`
- `python/eu_digital_lab/__init__.py`
- `python/tests/test_screen_ocr_policy.py`
- `tools/validate_screen_ocr_policy.py`
- `docs/03-contracts/SCREEN_OCR_CAPTURE_CONTRACT.md`
- `docs/04-adrs/ADR-0028-consented-screen-ocr-capability.md`
- `specs/SPEC-032-consented-ocr-capability.md`
- `contracts/README.md`
- `cpp/README.md`
- `docs/03-contracts/OBSERVATION_POLICY_CONTRACT.md`
- `docs/08-roadmap/ROADMAP.md`
- `contracts/fixtures/component_maturity.json`
- `REPOSITORY_TREE.txt`

## Testes executados

- `python3 tools/validate_screen_ocr_policy.py` — aprovado.
- `python3 -m unittest python.tests.test_screen_ocr_policy -v` — 4/4.
- `ctest --test-dir build/dev --output-on-failure -R screen_ocr_sensor` —
  aprovado.
- `python3 -m unittest discover -s python/tests -v` — 181/181.
- `ctest --test-dir build/dev --output-on-failure` — 14/14.
- Helper Windows nativo com MSVC/vcpkg — compilação e 14/14 CTest.
- Validação de maturidade de componentes — 21 componentes válidos.
- Validação híbrida, contratos, SPECs e documentação — aprovadas.

## Resultados

Captura sem consentimento, revogada, pausada ou sem solicitação é bloqueada
antes do armazenamento. Captura explícita e solicitação pontual autorizada
funcionam; somente a região permitida é persistida. Nenhum evento contém
pixels ou texto OCR bruto.

## Critérios de aceite

Todos os critérios da SPEC-032 passaram em Linux/WSL e Windows nativo. A SPEC
pode ser considerada concluída.

## Desvios

Não foi integrado um adaptador de captura Windows real nem um backend OCR
concreto. As portas continuam injetadas e locais; a seleção de backend e a
integração de produto pertencem às fases posteriores.

## Riscos e pendências

- O ledger de consentimento continua sendo resolvido pelo host/integrador; o
  sensor recebe a política resolvida e não concede consentimento por conta
  própria.
- A retenção efetiva dos payloads depende do consumidor de armazenamento da
  SPEC-030; o evento carrega a classe/duração normativa para essa aplicação.

## Decisões tomadas

- O modo `on_demand` exige correspondência exata entre o ID da política e o ID
  recebido na chamada.
- A região é recortada antes do `ImageStore`, enquanto o OCR recebe o frame
  original com a região autorizada para preservar coordenadas de origem.
- Falha OCR não é convertida em lista vazia de palavras; usa evento e health
  distintos.

## Evidências

- `valid consented screen OCR policies`.
- `Ran 181 tests ... OK`.
- `100% tests passed out of 14` no Linux.
- `100% tests passed out of 14` no Windows/MSVC.
- `Windows Runtime Preview validation passed.`
