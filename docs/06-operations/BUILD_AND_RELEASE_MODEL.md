# Modelo de Build e Release

## Ambientes

### Laboratório

- Python 3.12 ou superior;
- ambiente bloqueado por lockfile;
- dependências de treinamento e análise opcionais por grupo;
- execução local reproduzível;
- notebooks não são fonte normativa;
- toda lógica promovível deve existir também em módulo testável.

### Cérebro Implantado

- C++23;
- CMake Presets;
- MSVC e Clang-CL inicialmente;
- builds Debug, RelWithDebInfo e Release;
- sanitizers em plataformas compatíveis;
- instalador sem Python.

Na Fase 0.3, o primeiro artefato operacional é um host de console controlado
explicitamente. Serviço Windows, auto-start, interface gráfica e modelos
empacotados permanecem fora do pacote até possuírem SPEC própria.

## Pipelines de CI

```text
contracts
→ python lint/test
→ cpp configure/build/test
→ replay tests
→ cross-language equivalence
→ benchmark gates
→ packaging
```

## Tipos de release

- `lab-*`: versão do ambiente experimental;
- `runtime-*`: versão do Cérebro Implantado;
- `contract-*`: versão de schemas e protocolos;
- `model-*`: versão de modelo exportado;
- `dataset-*`: versão de corpus ou replay.

Uma release do runtime declara exatamente quais versões de contratos, modelos e promoções contém.
