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
- CTest: 23/23 passed, including `procedural_avatar`.
- Python suite: 205/205 passed.
- Targeted Ruff: passed.
- Contract, promotion, maturity and configuration validators: passed.
- Documentation validation and repository-tree freshness: passed.
- `git diff --check`: passed.
