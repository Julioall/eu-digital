# Qt Desktop Matrix (Windows 11)

Data da execução automatizada: 2026-08-04
Ambiente: Windows 11, Qt 6.7.2/LLVM-MinGW, Debug, plataformas `offscreen` e
`qwindows`, um monitor LG ULTRAGEAR 1920x1080 em 100% e pt-BR com layouts
`0416:00010416` e `0416:00000416` instalados.

| Cenário | Evidência automatizada | Estado físico |
|---|---|---|
| UI Automation e teclado | Em `qwindows`, o campo expõe `QAccessible::EditableText`, nome e descrição acessíveis; Tab move o foco sem ativar o avatar. | Narrator/NVDA e inspeção por cliente UI Automation externo ainda exigem execução interativa. |
| IME pt-BR | O Windows possui pt-BR e dois layouts 0416 instalados; `QInputMethodEvent` preserva e envia `ação, informação e João` pelo campo real. | Sequência física de dead keys no teclado/IME ainda não foi executada interativamente. |
| DPI 100–250% | A política `PassThrough` passou em `qwindows` no DPR físico 1.0 e com `QT_SCALE_FACTOR` 1.25, 1.5, 2 e 2.5; geometria, foco, tray, acessibilidade e input passaram em cada processo. | Apenas 100% foi observado como escala física do Windows; escalas não físicas são evidência de adaptação Qt, não de um monitor reconfigurado. |
| Multi-monitor | O probe consulta Qt, Windows Forms e WMI e encontrou de forma consistente uma tela ativa. | Movimento entre monitores com DPIs distintos permanece sem evidência porque a máquina expõe apenas um monitor. |
| System tray | `QSystemTrayIcon::isSystemTrayAvailable()` e visibilidade passaram no shell nativo; 200 ativações offscreen mantêm o p99 em `desktop_performance_samples.jsonl`. | Clique humano/menu nativo continua como verificação interativa recomendada. |
| Click-through e transparência | O HWND real possui `WS_EX_TRANSPARENT` e `WS_EX_NOACTIVATE`; o teste detectou e corrigiu a ausência inicial de `NOACTIVATE`. Flags Qt, janela frameless e alpha 0 também passam. | Inspeção visual de bordas/alpha pelo compositor físico permanece pendente. |
| Suspend/resume | `desktop_integration_test` injeta `WM_POWERBROADCAST`, interrompe captura no suspend e restaura somente sensores ainda consentidos no resume. | Ciclo Sleep/Hibernate físico permanece pendente. |
| Consumo idle | Processo desktop completo, sem sensor/modelo, medido por `GetProcessTimes`; limite <1% é gate do teste. | A amostra deve ser repetida em build Release antes de distribuição. |

Os resultados estruturados são emitidos em
`build/windows-qt/qt_windows_platform_probe*.json`. O teste nativo pode ser
repetido com:

```powershell
ctest --test-dir build/windows-qt -R "^qt_avatar_shell_windows" --output-on-failure
```

Os resultados automatizados comprovam invariantes de código e regressão no
Windows, não comportamento de hardware que o ambiente não expôs. A SPEC-050
permanece `in_progress` enquanto multi-monitor, tecnologia assistiva externa,
compositor visual e Sleep/Hibernate não tiverem evidência física; IME por dead
keys e escalas físicas diferentes de 100% também permanecem pendentes.
