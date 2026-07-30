# Contracts: Dialogue and Avatar

The SPEC-014 contracts are local and represent presentation, not personality:

- `dialogue_notice.schema.json`: question/notice with hypothesis, confidence,
  context and reason;
- `dialogue_feedback.schema.json`: user correction, defer or silence;
- `avatar_view_state.schema.json`: visual state with explicit non-focus,
  non-capture and non-blocking invariants.

The desktop host is an optional port. Its absence does not erase history and is
not treated as a negative observation.

## SPEC-042 headless presentation boundary

`avatar_presentation_profile.schema.json` is a versioned sidecar to
`avatar_view_state.schema.json`; it does not change the public 1.0 view-state
schema. The profile accepts only bounded procedural parameters: shape, density,
turbulence, glow, palette, speed and cohesion. Its source and override audit
fields are mandatory so a visual profile cannot be mistaken for emotion,
intention or a cognitive state.

The native headless renderer exposes consent, global pause, health, frame quota,
feedback controls and local history. It renders CPU-only deterministic frames
without a model, downloaded asset, focus, input capture or action port. This is
only the replaceable rendering core; a desktop host remains gated by
ADR-0032.

## SPEC-041 desktop boundary

The desktop spike is an optional development target. It does not change the
`AvatarPresenter`/`AvatarPresentationPort` boundary or the `AvatarViewState`
schema. Its result is a capability decision, not a product shell: the SDL2 and
Dear ImGui probe remains isolated, and unsupported accessibility, tray,
click-through and lifecycle behavior is recorded rather than emulated in the
dialogue controller.
