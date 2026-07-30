# SPEC-041 execution report

SPEC: SPEC-041 - Windows desktop interface spike
Agent: Codex
Date: 2026-07-30

## Increment

- Installed SDL2 2.32.10 and Dear ImGui 1.92.8 in the local vcpkg
  `x64-windows` development environment.
- Added the optional CMake target `desktop_interface_spike`; it is disabled by
  default and is not imported by the runtime or cognitive core.
- Exercised SDL video/events, a hidden high-DPI window, an ImGui context and
  frame lifecycle, text input initialization, display/DPI queries, opacity,
  input-grab state and bounded idle event processing.
- Produced a machine-readable Windows scenario matrix and a substitutive ADR
  before any product shell implementation.

## Evidence and gates

- MSVC Debug build of `desktop_interface_spike`: passed.
- Native probe execution: exit code 0; SDL 2.32.10 and ImGui 1.92.8.
- Matrix: 16 scenarios, 7 validated, 6 partial and 3 unsupported.
- Baseline/ablations: SDL2 without ImGui, ImGui-disabled treatment and
  transparency-disabled treatment are captured under `validation/ablations/`.
- Focus and clipboard invariants: validated.
- SDL2/ImGui core integration and idle event loop: validated.
- Fullscreen hidden-window probe: negative/partial result; the call did not
  return within the safety limit during the first probe and was removed from
  automated desktop-mutating execution.
- Screen reader/UI Automation, click-through and tray: unsupported by the
  tested SDL2/ImGui core path.
- Renderer boundary: the target is optional and does not alter
  `AvatarPresenter`/`AvatarPresentationPort` or `AvatarViewState`.

## Decision boundary

SDL2 plus Dear ImGui is not selected as the product shell. ADR-0032 proposes
Qt 6/QML as the substitutive candidate because the missing Windows accessibility
and shell-lifecycle capabilities must be provided by the future adapter. The
proposal requires human review and a manual Windows matrix before SPEC-042 can
start the shell implementation.

This report is engineering verification, not evidence of cognition,
personality, emotion or consciousness. No sensor, clipboard, model, action,
network connection or product shell was enabled.

## Artifacts

- Matrix: `docs/06-operations/DESKTOP_INTERFACE_SPIKE_MATRIX.md`
- ADR: `docs/04-adrs/ADR-0032-desktop-interface-substitution.md`
- Probe result: `validation/reports/desktop_interface_spike_windows.json`
- Source: `cpp/app/desktop_interface_spike.cpp`
