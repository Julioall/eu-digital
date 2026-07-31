---
id: SPEC-052
title: Interface de Bandeja, Widgets e Diálogo (Tray UI)
status: cancelled
phase: product_beta
dependencies: [SPEC-050, SPEC-040]
adrs: []
contracts: []
---

# SPEC-052 — Interface de Bandeja, Widgets e Diálogo (Tray UI)

Status: implemented
Owner: humano
Fase: product_beta  
Dependências: SPEC-050 (Minimum Usable Application), SPEC-040 (Local Model Dialogue)  
ADRs aplicáveis: Nenhuma  
Contratos afetados: Nenhum.

## Problema
Atualmente, o EU Digital opera como um processo silencioso de bandeja (`qt_tray_adapter.hpp`) que apenas exibe um ícone estático, oferecendo funcionalidades limitadas através de um menu de contexto padrão do S.O. Para cumprir sua premissa de ser um "parceiro cognitivo" (comunicativo, discreto, pró-ativo), o aplicativo precisa de uma interface gráfica que reflita estados, permita interações rápidas via chat e apresente resumos sem monopolizar a tela ou o fluxo de trabalho do usuário. O desenvolvimento do Avatar procedural final (renderização gráfica pesada) foi adiado para evitar gargalos arquiteturais nesta etapa.

## Objetivo
Desenvolver a suíte de interfaces gráficas nativas baseada nos conceitos visuais aprovados, utilizando Qt Widgets e estilos modernos, com foco em:
1. **Sistema de Ícones Dinâmicos:** Refletir estado (Ativo, Pensando, Perguntando, Pausado, Offline).
2. **Widget Compacto & Painel Rápido:** Flyout customizado ao clicar no ícone, exibindo o status do agente e as métricas principais do sistema.
3. **Widget Expandido (Chat):** Interface de conversação texto-a-texto conectada ao backend cognitivo e modelo (Ollama local).
4. **Notificações:** Popups contextuais (toasts) ancorados no sistema ou renderizados customizadamente na tela.

## Resultado observável
Ao iniciar o aplicativo, o ícone da bandeja será dinâmico. O clique simples abrirá um Widget Customizado contendo o campo de texto rápido e resumos de status, ao invés do menu padrão. A digitação e submissão abrirão o painel estendido com o histórico do chat. O design seguirá as diretrizes visuais (dark mode, cantos arredondados, fontes modernas) de forma leve. A renderização do avatar animado (partículas) será substituída temporariamente por ícones fixos de estado ou animações sutis de UI (CSS/QSS puro).

## Requisitos funcionais
- O `QtTrayAdapter` deve migrar de um `QMenu` nativo genérico para o controle de um `QWidget` sem bordas (`Qt::Popup` ou `Qt::Tool`) posicionado próximo à bandeja do sistema.
- **Painel Rápido / Widget Compacto**: Mostrar as estatísticas do `RuntimeHost` (número de sensores ativos, total de memórias).
- **Interface de Chat**: 
  - Campo de input, histórico de mensagens (ListView ou ScrollArea).
  - Capacidade de "minimizar" para o widget compacto.
- **Eventos Cognitivos**: O chat deve consumir eventos emitidos pelo `CognitiveCoordinator` e `EventBus` para renderizar as falas do modelo.
- **Configurações**: Janela modal para ajustes de auto-início, limites e tolerâncias de IA (mockado visualmente por enquanto, focado na persistência em `QSettings`).

## Requisitos não funcionais
- **Design Visual (QSS/Estilização):** Implementar o visual escuro (Dark Mode) de forma nativa utilizando QSS (Qt Style Sheets). Utilizar cores coerentes com os *mockups* (fundos em tons #1A1A1A a #252525, acentos azuis e bordas sutis).
- **Responsividade Gráfica:** A interface gráfica não pode causar picos de processamento. A troca de layout do Widget Compacto para Expandido (animação) deve operar a 60fps constantes sem travar o processamento paralelo.
- **Desacoplamento UI-Lógica**: A GUI (`QWidget`) e a camada Controladora (`DesktopController`) devem se comunicar via sinais Qt (`signals/slots`), isolando a interface da manipulação das Threads de backend.

## Entradas
- Eventos de Input do Chat (usuário digita texto e envia).
- Cliques de botões na UI (pausar, ver logs, fechar).
- Eventos de estado vindos do `CognitiveCoordinator`.

## Saídas
- Histórico de mensagens visível.
- Alterações visuais nos estados do ícone (Troca de `.svg` / `.png`).
- Notificações transitórias (Toast messages) quando o backend emite eventos proativos.

## Fluxo
1. `DesktopController` instancia `QtTrayInterface` (novo componente que encapsula o ícone da bandeja, menu de contexto nativo fallback e o *Flyout Widget* principal).
2. Ao clicar no tray icon (botão esquerdo), o app calcula as coordenadas absolutas na tela e exibe o *Compact Widget*.
3. O usuário digita e submete a mensagem. A UI emite o sinal (ex: `userInputReceived(QString)`).
4. A UI transiciona (redimensiona e revela) para o modo *Expanded Widget*.
5. O `DesktopController` capta o sinal de mensagem e publica o CanonicalEvent (`user.utterance`).
6. Quando o `RuntimeHost` processa e o modelo gera a resposta (`agent.utterance`), o `DesktopController` envia via slot (`appendMessage(role, text)`) para a interface atualizar o chat.

## Estados e transições
- `TrayHidden`: Apenas o ícone está visível.
- `CompactVisible`: Janela flutuante pequena próxima ao tray, apenas estado e prompt.
- `ChatVisible`: Janela expandida, contendo o log do diálogo.
- `SettingsVisible`: Janela separada bloqueante ou não com configurações de sistema.

## Critérios de aceite
- [ ] O `eu_digital_desktop.exe` utiliza QSS (Qt Style Sheets) para recriar a identidade visual dos *mockups* (Dark Mode premium, bordas arredondadas, sombras, placeholders de avatar).
- [ ] O ícone do System Tray reflete o estado do aplicativo.
- [ ] Clique esquerdo no tray exibe a interface *Flyout* próximo ao relógio.
- [ ] O usuário consegue interagir via chat com as respostas retornadas do `CognitiveCoordinator` e `RuntimeHost`.
- [ ] A performance de uso de memória e CPU não sobe em momentos passivos da interface, garantindo respeito às threads background.

## Plano de testes
- **Unitários**: Testar as emissões de Sinais (`signals`) dos botões da interface isoladamente.
- **Integração UI-Controller**: Verificar se mensagens postadas no `EventBus` pela thread cognitiva são refletidas corretamente nos painéis da UI via mecanismo seguro cross-thread.

## Migração
- Reposicionar e refatorar `QtTrayAdapter` (`qt_tray_adapter.hpp`) para servir à nova arquitetura de Widget/Flyout, ao invés de um simples QMenu.


## Escopo negativo
- Nada a declarar.
