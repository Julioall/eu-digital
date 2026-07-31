# Plano de Execução: SPEC-050 (Minimum Usable Application)

## Objetivo
Empacotar o "Cérebro" e o "Corpo" (Avatar/Tray) em um único processo desktop multithread seguro e sem travamentos.

## Etapas Verificáveis

### Etapa 1: Entrypoint Desktop
- Criar `cpp/app/eu_digital_desktop.cpp` (será o novo binário principal final do pacote MSIX, substituindo o `eu_digital_runtime` como o arquivo que o usuário final de fato roda).

### Etapa 2: Multithreading Architecture
- Instanciar o `QApplication` e o Event Loop do Qt na Thread Principal (Main Thread).
- Subir uma `std::thread` dedicada rodando o `RuntimeHost::start()` bloqueante.
- Garantir desligamento limpo usando atômicos (`std::atomic_bool` flag de saída).

### Etapa 3: Signal/Slots Thread Safe
- Usar `QMetaObject::invokeMethod` ou mecanismos equivalentes thread-safe do Qt para que a thread cognitiva dispare o comando "Falar()" e a GUI obedeça sem causar corrupção de memória (Data Race).
- Validar via ThreadSanitizer.

## Arquivos Prováveis
- `cpp/app/eu_digital_desktop.cpp`
- `CMakeLists.txt` (Target atualizado)
