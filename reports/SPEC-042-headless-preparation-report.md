# SPEC-042 headless preparation report

Status: partial preparation only; SPEC-042 remains future.

## Scientific protocol for this increment

- Hypothesis: a bounded procedural renderer can produce deterministic local
  frames while preserving the non-blocking, consent and resource controls.
- Baseline: hidden view or missing consent produces no frame and does not
  fabricate a negative observation.
- Metrics: deterministic pixel equality for replay, rendered/dropped frame
  counts, explicit health reason and invariant flags for focus/input/blocking.
- Ablations: consent disabled, global pause and exhausted frame quota.
- Falsification: unbounded profile values accepted, frame output differs for
  the same input, or any path enables focus, input capture or work blocking.

## Implemented

- Added the versioned `AvatarPresentationProfile` sidecar contract.
- Added a dependency-free native CPU renderer for particles, filament, smoke
  and metaball profiles.
- Added the `avatar_frame.schema.json` output contract and a CLI probe that
  reports frame metadata plus a local framebuffer digest.
- Added bounded profile validation, deterministic frame output, consent,
  global pause, health, quota, feedback controls and local feedback history.
- Added an optional capability descriptor with absence, failure, removal,
  reinstall and substitution coverage.

## Explicitly not claimed

- No desktop window, Qt/QML adapter or product shell was added.
- No accessibility, tray, DPI, focus or lifecycle matrix was claimed.
- No model, AI-generated asset, input capture, clipboard access or action port
  was introduced.
- SPEC-042 acceptance criteria remain open because ADR-0032 still requires
  human review and a manual Windows matrix before the host adapter.

## Verification target

The native `procedural_avatar` CTest and the focused Python profile-schema tests
cover this preparation increment. The full SPEC gate is intentionally pending.

## Verification executed

- C++ build with the explicit MSVC environment: passed.
- CTest: 24/24 passed, including `procedural_avatar` and
  `procedural_avatar_probe`.
- Python suite: 207/207 passed.
- Targeted Ruff: passed.
- Contract, promotion, maturity and configuration validators: passed.
- Documentation validation and repository-tree freshness: passed.
- `git diff --check`: passed.

The CLI probe renders a 64x64 frame with `model_required: false`, zero focus,
zero input capture and zero work blocking. Its output is validated by the
Python shared-schema test; it remains an automated headless probe, not manual
Windows shell evidence. The current local probe digest is
`79ae8824f21a7a7402ec543cc512a573d43e4bef83007a8076948a6d28eb506a`, with
1475 nonzero pixels out of 4096.
