---
id: SPEC-050
title: Desktop Application and Vertical Slice
status: draft
phase: design
dependencies: [SPEC-045, SPEC-042, SPEC-028]
adrs: []
contracts: []
---

# SPEC-050 — Desktop Application and Vertical Slice

Status: draft  
Owner: humano  
Fase: design  
Dependências: SPEC-045 (Integrated Cycle), SPEC-042 (Avatar Shell), SPEC-028 (Native Shell)  
ADRs aplicáveis: Nenhuma  
Contratos afetados: Nenhum.

## Problema
Toda a infraestrutura do ciclo cognitivo (SPECs 045 a 049) ainda forma apenas uma biblioteca ou executável *headless*. O Shell em Qt (SPEC-042) existe em isolamento. O projeto visa construir um produto aplicacional (Vertical Slice) coeso que rode no desktop do usuário e coordene GUI, sensores de O.S., ciclo cognitivo e gerência do modelo de maneira thread-safe, com critérios de UX estritos.

## Objetivo
Empacotar o Runtime cognitivo multithread com o Shell Qt existente no binário final `eu_digital_desktop.exe`. Definir de forma explícita os diferentes modos da UI e cobrir fluxos primordiais de produto: onboarding, consentimento explícito pré-observação, pause/resume global, visualização de diagnósticos/quota e degradação suave na ausência de modelo.

## Resultado observável
O usuário final inicia a aplicação via menu Iniciar (Windows). Uma tela de Onboarding solicita consentimento de observação local. Ao aprovar, o app vai para a bandeja do sistema (Tray). O *frame time* do Avatar é mantido estável (p95 < 16ms), e o tempo de resposta do ícone da bandeja é menor que 50ms, independentemente da carga do modelo local operando em background.

## Requisitos funcionais
- Ponto de Entrada unificado (`eu_digital_desktop.cpp`).
- Separação Thread GUI (Qt Event Loop) vs. Thread Cognitiva (Coordenador).
- Implementar Modos de Interface explícitos para resolver o conflito da SPEC-042:
  - **Passivo**: Avatar click-through e transparente.
  - **Interativo**: Janela de diálogo/confirmação bloqueante sobre o evento que chamou a interação.
  - **Painel**: Menu na bandeja contendo Onboarding, Pause, Status, Diagnóstico.
- Fluxo de Onboarding obrigatório bloqueando o início dos sensores.
- Recuperação em Crash: Se fechar inesperadamente, o app sobe em modo Degradado e avisa na bandeja.

## Requisitos não funcionais
- **Performance Gráfica**: Frame time p95 < 16.6ms e p99 < 33ms na renderização da interface, atestando desacoplamento real de CPU.
- **Responsividade**: O menu da bandeja deve aparecer em < 50ms após o clique, garantindo ausência de Hang.
- **Consumo Idle**: O processo estático sem processar eventos e sem modelo carregado ativamente não deve exceder baseline de recursos do SO (CPU < 1%).
- **Inicialização e Encerramento Limpos**: O processo deve morrer graciosamente ao fechar a sessão do Windows (graceful shutdown intercept).

## Entradas
- Eventos de Input do Usuário na GUI e Cliques do Tray.
- Inicialização do Processo de Usuário (Desktop).

## Saídas
- Interface QML e Logs unificados.

## Fluxo
1. Processo inicia. Main thread assume o `QApplication`.
2. Lê flag de Onboarding/Consentimento. Se `false`, invoca Dialog Modal. Sensoriamento pausado.
3. Se `true`, invoca a Thread Cognitiva passando a instância do Registry.
4. UI entra em modo *Passivo* e se aloja no Tray.
5. Em caso de *PresentationPort* requisitar fala: UI entra temporariamente em modo *Interativo*.

## Estados e transições
- `onboarding`: Janela forçada de consentimento, sensores offline.
- `running`: Background, modo passivo, sensores online.
- `paused`: Background, sensores offline por ordem do usuário.
- `interactive`: Focus lock na janela de diálogo ativa.
- `degraded`: Rodando mas relatando falhas graves no painel de diagnóstico (ex: sem LLM).

## Erros esperados
- `ThreadHangError`: (Deve ser detectável nos testes de integração, falhando a build se ocorrer deadlock entre GUI e Core).

## Escopo negativo
- Não recriar componentes QML.
- Não exportar telemetria online do produto (Constituição proíbe nuvem).
- Não embutir o modelo de vários gigabytes no executável (será baixado ou carregado via gateway local separado ou gerência de modelos do usuário).

## Critérios de aceite
- [ ] Tempo de resposta ao clique no Tray é inferior a 50ms (p99).
- [ ] O aplicativo inicializa sem modelo presente de maneira graciosa (Entra em `degraded` e avisa na bandeja).
- [ ] Ao clicar em "Pausar Observação" na bandeja, os sensores desativam o pipeline e o consumo de CPU estaciona.
- [ ] Nenhum *deadlock* multithread detectado via ThreadSanitizer no ciclo completo (Onboarding -> Run -> Exit).
- [ ] End-to-end Test provando que a primeira execução sem consentimento bloqueia a observação e a captura global.

## Plano de testes

### Unitários
- Mockar a UI e a Thread Cognitiva, testar a semântica da máquina de estados (Passivo <-> Interativo).

### Integração
- Teste End-to-End: Subir aplicação `headless` conectada a `xvfb` ou similar, mandar evento e checar log de Frame Time p95/p99 via QTestLib.

### Contrato
- Sinais Qt (Signal/Slot) garantem transferência thread-safe imutável (DTO).

### Desempenho
- Frame times registrados e rejeição automática se ultrapassar baseline no CI.

### Recuperação
- Testar religamento após crash e verificação do status no Painel.

## Migração
- Inserir tabela de `app_preferences` para flag de onboarding/consent.

## Rollback
- Reverter o CMake para compilar os sub-sistemas como lib estática sem gerar o executável desktop integrado.

## Evidências de conclusão
- Vídeo do Vertical Slice ou dump de métricas de CI com as estatísticas de latência provando desacoplamento GUI-Core.
