# Plano de Execução: SPEC-050 (Desktop Application and Vertical Slice)

## Pré-condições
- SPEC-042 (Avatar Shell C++) totalmente funcional.
- Coordenador Headless (SPEC-045) sem dependências circulares com GUI.

## Contratos Congelados
- Nenhum. O binário de app atua apenas consumindo e instanciando os sistemas abstratos.

## Arquivos por Etapa

### Etapa 1: Entrypoint
- **Ação:** Instanciar Application, Runtime e Thread de Background.
- **Arquivos:**
  - `cpp/app/eu_digital_desktop.cpp`
  - `CMakeLists.txt` (Adicionar novo executable target e remover menções antigas de boot via `eu_digital_runtime` para produção).

### Etapa 2: Máquina de Estados de UI
- **Ação:** Mapear sinais/slots para alternar a interface entre Passivo, Interativo e Painel.
- **Arquivos:**
  - `cpp/shell/avatar_controller.hpp` (Mapear a lógica de toggle).
  - `cpp/tests/avatar_controller_test.cpp` (Mockar chamada thread-safe).

### Etapa 3: Consentimento e Onboarding
- **Ação:** Bloquear a incialização dos sensores (SystemActivitySensor) até ler flag true no sqlite.
- **Arquivos:**
  - `cpp/core/privacy_storage.hpp`
  - `cpp/app/onboarding_flow.hpp`

## Comandos de Validação
```bash
cmake --build build/windows-msvc --target eu_digital_desktop
ctest --test-dir build/windows-msvc -R AvatarController --output-on-failure
```

## Migrações
- Inserir seed SQL para a flag `onboarding_completed=0`.

## Rollback
- Reverter o CMake para deixar apenas as bibliotecas.

## Evidências Esperadas
- Frame time log no artefato de teste < 16ms (p95).
- Teste E2E falhando intencionalmente a subida de sensores se onboarding não for completado.

## Critérios para Parar
- Se surgir conflito de TLS (Thread Local Storage) no Qt durante a invocação da thread cognitiva, interromper e utilizar `QThread` no lugar de `std::thread` para o ciclo.
