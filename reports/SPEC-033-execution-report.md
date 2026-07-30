# SPEC-033 execution report

SPEC: SPEC-033 - Native episode segmentation promotion
Agent: Codex
Date: 2026-07-30
Commit: isolated SPEC-033 commit

## Increment

- The frozen Python reference and the C++ candidate remain deterministic and
  isolated behind a removable CapabilityDescriptor.
- The promotion manifest records hashes for the Python entrypoint, development
  equivalence fixture set and locked holdout.
- The validator now runs on POSIX and Windows, validates every native episode
  against `episode.schema.json`, checks invariants and metamorphic properties,
  and records baseline, ablation and p50/p95/maximum operational metrics.

## Evidence

- Development equivalence: 4/4 cases, exact semantic match.
- Locked holdout equivalence: 2/2 cases, exact semantic match.
- Ground truth metrics: precision 1.0, recall 1.0, F1 1.0 and WindowDiff 0.0
  for both development treatment and holdout.
- Time-only baseline and context-disabled ablation are recorded separately;
  these operational/scientific metrics are not cognition claims.
- Windows measurement: p50 10.15 ms, p95 11.19 ms, maximum 14.12 ms, validator
  working set 36.27 MiB and approximately 389 cases/second.
- Metamorphic replay and missing-context checks passed.

## Gates executed

- C++ build and native CTest passed (22/22 in the current Windows build).
- Windows promotion validator passed: equivalence, schema, holdout, hashes,
  invariants, metamorphic checks and performance.
- Python unit tests (201/201), targeted Ruff for SPEC-033 files, mypy, contracts,
  component maturity, configuration and documentation validation passed.
- The repository-wide Ruff command still reports pre-existing findings in
  unrelated files; no unrelated refactoring was included in this SPEC commit.

## Decision boundary

The component remains `reference_status: frozen`, `native_status: equivalent`
and `product_status: unavailable`. The promotion was not inserted into
`promotions/registry.json`; a human review with an `approval_review_id` is
required. Cross-language agreement and operational metrics are verification
evidence, not scientific or ecological validity.

No model, sensor, actuator, network connection or product task was added.

## Artifacts

- `validation/reports/episode_segmentation_v1.json`
- `validation/reports/episode_segmentation_v1_divergences.json`
- `promotions/cognition.episode_segmentation.v1.json`
- `validation/holdout/manifest.json`
- `docs/03-contracts/EPISODE_PROMOTION_CONTRACT.md`
