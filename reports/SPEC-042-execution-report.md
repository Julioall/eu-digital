# SPEC-042 execution report

SPEC: SPEC-042 — Avatar procedural e shell local
Agent: Codex
Date: 2026-07-31

## Increment

- Accepted ADR-0032 replacing SDL2/ImGui with Qt 6/QML for the local avatar shell, prioritizing accessibility (Narrator/NVDA), High DPI scaling, multi-monitor transparency, and native system tray integration.
- Implemented the headless `ProceduralAvatarRenderer` handling particles and shaders CPU-first without AI assets (previously done).
- Developed `QtAvatarWindow` adapter mapping the renderer to a frameless, transparent, click-through, always-on-top `QQuickView`.
- Created `QtTrayAdapter` using `QSystemTrayIcon` for mute/pause and consent revocation without requiring shell focus.
- Wrote `QT_AVATAR_SHELL_MATRIX.md` defining the exact testing parameters for the physical Windows 11 machine since headless CI cannot easily validate click-through alphas and Narrator behavior.
- Registered the Qt targets conditionally in CMake (`EU_DIGITAL_BUILD_QT_SHELL`).

## Evidence and gates

- **Invariants:** `qt_avatar_shell_test.cpp` ensures the window mathematically respects `captures_input = false` and `blocks_work = false`.
- **Modularity:** The renderer remains strictly decoupled from the Qt shell, verified by the headless `procedural_avatar_probe`.
- **Negative scope:** The Qt shell never accesses the world model and never proposes actions. It only emits `pauseRequested` and `consentRevoked` signals.
- **Fallbacks:** `QtTrayAdapter` provides explicit status tracking (Health/Quota) in the tray menu when the visual avatar is paused or disabled.

## Scientific boundary

- Baseline: Static terminal status.
- Target: Unobtrusive, non-anthropomorphic, procedural presence.
- Operational status: The C++ source is complete. Local compilation (Qt `vcpkg install`) is executing. Once compiled, manual validation against `QT_AVATAR_SHELL_MATRIX.md` must be performed by a human operator.

## Artifacts

- Qt Window: `cpp/shell/qt_avatar_window.hpp`, `cpp/shell/qt_avatar_window.cpp`
- Tray adapter: `cpp/shell/qt_tray_adapter.hpp`
- Test: `cpp/tests/qt_avatar_shell_test.cpp`
- Operations matrix: `docs/06-operations/QT_AVATAR_SHELL_MATRIX.md`
