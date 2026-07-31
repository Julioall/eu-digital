# Qt Avatar Shell Matrix (Windows 11)

Esta matriz documenta a validação manual do adaptador Qt 6/QML exigida pela ADR-0032 e SPEC-042, confirmando que as lacunas encontradas no spike SDL2/ImGui foram resolvidas.

## Critérios de Validação

| Cenário | Requisito | Status | Observação |
|---|---|---|---|
| **Acessibilidade (UI Automation)** | A janela deve expor sua árvore para o Narrator/NVDA sem capturar foco de teclado. | Pendente Qt build | Usa o módulo `QtAccessibility`. |
| **IME / Idiomas** | Composição de teclado (ex: pt-BR, JP) não pode ser interceptada globalmente. | Pendente Qt build | O Qt gerencia IME adequadamente quando sem foco. |
| **DPI Scaling** | O avatar deve manter proporção em monitores de 100% a 250% de escala (High DPI). | Pendente Qt build | Requer `QGuiApplication::setHighDpiScaleFactorRoundingPolicy`. |
| **Multi-monitor** | Movimentar a janela transparente entre monitores de DPIs diferentes não deve causar flickering. | Pendente Qt build | Suportado nativamente pelo Qt 6. |
| **System Tray** | O ícone da bandeja deve responder a cliques e exibir o menu de pausa/consentimento. | Pendente Qt build | Implementado via `QSystemTrayIcon` em `qt_tray_adapter`. |
| **Click-through** | Cliques de mouse sobre o avatar transparente devem passar para o app abaixo (ex: VS Code, Chrome). | Pendente Qt build | Garantido pelas flags `Qt::WindowTransparentForInput` e frameless. |
| **Transparência / Alpha** | O fundo do avatar deve ser 100% transparente (alpha 0) sem bordas escuras (pre-multiplied alpha). | Pendente Qt build | Suportado por `setColor(Qt::transparent)`. |
| **Lifecycle (Suspend/Resume)** | Retornar de Sleep/Hibernate do Windows não deve quebrar o contexto de renderização (loss of device). | Pendente Qt build | Qt recria a swapchain/contexto automaticamente se necessário. |
| **Consumo Idle** | O QTimer e o renderer não devem acordar a CPU desnecessariamente quando não há frames (ex: avatar silenciado). | Pendente Qt build | `frame_timer_` pausa se renderer não tiver frames pendentes. |

> **Nota:** Esta matriz será validada fisicamente em um ambiente Windows 11 com hardware real após a compilação bem-sucedida do pacote Qt6 pelo `vcpkg`.
