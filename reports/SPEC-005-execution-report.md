# Relatório de Execução

SPEC: SPEC-005  
Agente: Codex  
Data: 2026-07-28  
Commit: trabalho local não commitado

## Alterações realizadas

- implementadas as portas locais `ImageStore` e `OcrEngine`;
- implementada política de hash perceptual, supressão de duplicatas e captura
  novamente por intervalo;
- persistidas referências de imagem, texto OCR, coordenadas e confiança sem
  copiar pixels para os eventos;
- preservado o evento visual quando o OCR falha, com estado de saúde e erro
  estruturados;
- adicionado `CapabilityDescriptor` e `ScreenOcrPlugin`;
- adicionados testes C++ determinísticos para os três critérios de aceite.

## Arquivos modificados

- `cpp/core/screen_ocr_sensor.hpp`;
- `cpp/tests/screen_ocr_sensor_test.cpp`;
- `CMakeLists.txt`;
- `cpp/README.md`;
- `docs/06-operations/DEVELOPMENT_COMMANDS.md`;
- `specs/SPEC-005-screen-ocr-sensor.md`.

## Testes executados

```text
cmake -S . -B build/spec005-isolated -G Ninja -DCMAKE_CXX_COMPILER=x86_64-linux-gnu-g++-13 -DCMAKE_CXX_FLAGS="--sysroot=/tmp/tmp.21tTxQm2JN/extracted"
cmake --build build/spec005-isolated -j2
ctest --test-dir build/spec005-isolated --output-on-failure
PYTHONPATH=python python3 -m unittest discover -s python/tests -q
PYTHONPATH=python python3 tools/validate_contracts.py
PYTHONPATH=python python3 tools/validate_sandbox.py datasets/synthetic/v1
PYTHONPATH=python python3 tools/validate_hybrid.py --skip-build
/tmp/tmp.HyT0IpIN6S/pkg/ruff-0.16.0.data/scripts/ruff check python/eu_digital_lab/evaluation.py python/eu_digital_lab/promotion.py python/eu_digital_lab/validation.py python/tests/test_evaluation.py python/tests/test_promotion.py python/tests/test_validation.py
PYTHONPATH=/tmp/tmp.21tTxQm2JN/extracted/usr/lib/python3/dist-packages:python /tmp/tmp.21tTxQm2JN/extracted/usr/bin/mypy python/eu_digital_lab
```

## Resultados

- teste específico de tela/OCR aprovado;
- suíte CTest completa: 7 testes aprovados;
- suíte Python: 60 testes aprovados;
- contratos e corpus sintético validados;
- validador híbrido, Ruff direcionado e mypy aprovados.

## Critérios de aceite

- [x] telas quase idênticas não geram OCR redundante;
- [x] texto e coordenadas são persistidos;
- [x] falha de OCR preserva o evento visual.

## Desvios

O motor OCR concreto e o capturador de plataforma permanecem atrás das
interfaces locais `OcrEngine` e `ImageStore`; o teste usa implementações
determinísticas em memória. Não foi adicionada interpretação semântica nem
vídeo contínuo, e nenhum serviço externo foi introduzido.

## Riscos e pendências

- a implementação concreta de captura de tela e OCR para o ambiente instalado
  requer validação específica de plataforma e qualidade do modelo local;
- o hash atual é um average hash de até 64 amostras de luminância fornecidas
  pelo capturador;
- calibração do limiar e do intervalo deve ser medida antes de uso operacional.

## Decisões tomadas

- usar portas abstratas para manter o núcleo independente de sensores e
  modelos concretos;
- manter bytes de imagem no armazenamento de captura, referenciados por path e
  hashes nos eventos;
- aceitar a captura visual mesmo quando OCR falha, marcando a saúde como
  indisponível para OCR.

## Evidências

- implementação: `cpp/core/screen_ocr_sensor.hpp`;
- teste: `cpp/tests/screen_ocr_sensor_test.cpp`;
- CMake/CTest: `CMakeLists.txt`;
- operação/documentação: `docs/06-operations/DEVELOPMENT_COMMANDS.md`.
