# Qt Desktop Matrix (Windows 11)

Data da execução automatizada: 2026-08-04
Ambiente: Windows, Qt 6.7.2/LLVM-MinGW, Debug, plataforma offscreen, uma tela
exposta pelo plugin Qt.

| Cenário | Evidência automatizada | Estado físico |
|---|---|---|
| UI Automation e teclado | `qt_avatar_shell_test` obtém uma interface `QAccessible` para o tray; o avatar usa `WindowDoesNotAcceptFocus` e `WindowTransparentForInput`. | Narrator/NVDA e navegação completa ainda exigem execução interativa. |
| IME pt-BR | O shell usa o caminho de input do Qt e o avatar não aceita foco; o sensor global observa sem consumir a mensagem. | Composição real pt-BR ainda não executada interativamente. |
| DPI 100–250% | A política é `PassThrough`; o probe valida DPI lógico e `devicePixelRatio` positivos para toda tela enumerada. | Matriz física 100%, 125%, 150%, 200% e 250% ainda pendente. |
| Multi-monitor | O probe percorre todas as telas fornecidas pelo Qt. | Ambiente atual expôs uma tela; movimento entre monitores com DPIs distintos permanece pendente. |
| System tray | 200 ativações reais do `TrayWidget` offscreen; p99 registrado em `desktop_performance_samples.jsonl`. | Clique/menu nativos continuam como verificação interativa recomendada. |
| Click-through e transparência | Flags frameless, always-on-top, transparent-for-input e does-not-accept-focus, além de alpha 0, são verificadas no objeto Qt real. | Inspeção visual de bordas/alpha em compositor físico permanece pendente. |
| Suspend/resume | `desktop_integration_test` injeta `WM_POWERBROADCAST`, interrompe captura no suspend e restaura somente sensores ainda consentidos no resume. | Ciclo Sleep/Hibernate físico permanece pendente. |
| Consumo idle | Processo desktop completo, sem sensor/modelo, medido por `GetProcessTimes`; limite <1% é gate do teste. | A amostra deve ser repetida em build Release antes de distribuição. |

Os resultados automatizados comprovam invariantes de código e regressão no
Windows, não comportamento de hardware que o ambiente não expôs. A SPEC-050
permanece `in_progress` enquanto IME, DPI extremo, multi-monitor, tecnologia
assistiva, compositor e Sleep/Hibernate não tiverem evidência física.
