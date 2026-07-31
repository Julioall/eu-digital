# ADR-0032 - Substitutive desktop interface after the SDL2/ImGui spike

Status: accepted
Date: 2026-07-30
Accepted: 2026-07-31
Supersedes prospective SDL2/Dear ImGui selection for the product shell

## Context

SPEC-041 required a technical check of SDL2 plus Dear ImGui before fixing a
desktop renderer. The isolated Windows probe successfully initialized SDL2 and
Dear ImGui, preserved the non-focus/non-clipboard boundary and exercised the
basic event loop. It did not provide a Windows UI Automation tree, tray adapter
or click-through contract. A hidden-window fullscreen probe also failed to
return within the safety limit. The complete matrix is in
`docs/06-operations/DESKTOP_INTERFACE_SPIKE_MATRIX.md`.

## Decision

Do not select SDL2 plus Dear ImGui as the product desktop shell. Open this ADR
as the substitutive decision gate and carry the product-shell candidate forward
as Qt 6/QML behind the existing `AvatarPresenter`/`AvatarPresentationPort`
boundary.

The future Qt adapter must prove, on Windows, IME/pt-BR, 100-250% DPI,
multi-monitor movement, keyboard navigation, UI Automation/screen-reader
exposure, tray lifecycle, controlled transparency/click-through, fullscreen,
suspend/resume, focus ownership and idle consumption. It must remain optional,
not import concrete UI code into the cognitive core, and publish only validated
`AvatarViewState` values.

No Qt dependency is added to the deployed runtime by this ADR. SPEC-042 may
implement the adapter only after this proposal is reviewed and the manual
matrix is captured. Until then, the SDL2/ImGui target remains a development
spike and is not a product capability.

## Consequences

- the renderer remains replaceable and the SPEC-014 contracts remain unchanged;
- the known SDL2/ImGui accessibility and shell-lifecycle gaps are not hidden;
- Qt 6/QML becomes a reviewable candidate rather than an implicit dependency;
- the product remains without a desktop shell until the required Windows matrix
  passes.

## Reversal

If a later validated adapter satisfies the matrix with another toolkit, this
ADR can be superseded without changing `AvatarViewState`, dialogue notices or
feedback contracts.
