# SPEC-041 Windows desktop interface spike matrix

The optional probe was built with SDL2 2.32.10 and Dear ImGui 1.92.8 using the
MSVC Debug toolchain. It creates a hidden, resizable high-DPI SDL window and an
ImGui context, so the probe itself does not take focus or modify the desktop.

The authoritative machine-readable result is
`validation/reports/desktop_interface_spike_windows.json`.

| Scenario | Result | Evidence or limitation |
|---|---|---|
| SDL2 initialization | validated | Video and event subsystems initialized. |
| Dear ImGui context | validated | Context, keyboard navigation and frame lifecycle initialized. |
| Focus | validated | Hidden window did not own input focus. |
| Clipboard | validated | Probe performs no clipboard read or write. |
| IME / pt-BR | partial | SDL text input enabled; physical composition needs manual Windows coverage. |
| Keyboard | partial | ImGui keyboard navigation enabled; physical layout needs manual coverage. |
| DPI 100-250% | partial | Display bounds/DPI query works; monitor scale matrix needs physical coverage. |
| Multiple monitors | partial | One display enumerated in this session; cross-monitor movement needs manual coverage. |
| Transparency | validated | SDL window opacity accepted. |
| Input grab | validated | Mouse and keyboard grabs disabled. |
| Fullscreen | partial | Hidden-window fullscreen probe did not return within the safety limit; no desktop mutation is used in automation. |
| Screen reader | unsupported | SDL2/Dear ImGui core does not provide a Windows UI Automation tree. |
| Click-through | unsupported | SDL2 core does not provide the required layered-window contract. |
| Tray | unsupported | SDL2/Dear ImGui core has no notification-area adapter. |
| Suspend/resume | partial | Event loop is pumpable; native lifecycle integration remains unimplemented. |
| Idle event loop | validated | Bounded idle pump remained responsive; measured result is in the JSON report. |

The partial and unsupported results are deliberate negative evidence. SDL2 plus
Dear ImGui is not promoted as the product shell. The substitutive decision is
recorded in `docs/04-adrs/ADR-0032-desktop-interface-substitution.md`; no shell
implementation starts until that decision and its manual accessibility matrix
are reviewed.

The protocol ablations are also captured locally:

- `validation/ablations/desktop_interface_spike_baseline.json`: SDL2 window
  without ImGui;
- `validation/ablations/desktop_interface_spike_no_imgui.json`: same interface
  with ImGui disabled;
- `validation/ablations/desktop_interface_spike_no_transparency.json`: ImGui
  treatment with window opacity disabled.

## Reproduction

```powershell
cmake -S . -B build/windows-msvc-vcpkg -G Ninja `
  -DEU_DIGITAL_BUILD_DESKTOP_INTERFACE_SPIKE=ON `
  -DSDL2_DIR=C:/Users/Julio/vcpkg/installed/x64-windows/share/SDL2 `
  -Dimgui_DIR=C:/Users/Julio/vcpkg/installed/x64-windows/share/imgui
cmake --build build/windows-msvc-vcpkg --target desktop_interface_spike --config Debug
$env:PATH = "C:\Users\Julio\vcpkg\installed\x64-windows\debug\bin;$env:PATH"
.\build\windows-msvc-vcpkg\desktop_interface_spike.exe `
  validation/reports/desktop_interface_spike_windows.json
```
