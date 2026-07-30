# SPEC-034 execution report

SPEC: SPEC-034 - Native episodic memory promotion
Agent: Codex
Date: 2026-07-30
Commit: isolated SPEC-034 commit

## Increment

- The existing C++ store remains atomic by `episode_id`, preserves the first
  record on duplicate input and exposes a removable CapabilityDescriptor.
- Context, document, modality, session, temporal and optional local embedding
  queries are compared with the frozen Python reference.
- Explicit similarity relations carry reason codes and source event IDs; no
  summary, fact or semantic consolidation is generated.
- The validator now records the Python reference hash, validates input and
  retrieved episodes against `episode.schema.json`, checks holdout separation,
  retention/provenance invariants, deterministic replay and a real
  context/embedding-disabled ablation.

## Evidence

- Development equivalence: 4/4 cases, numeric semantic match.
- Locked holdout equivalence: 2/2 cases, numeric semantic match.
- Ground truth metrics: development recall@k 1.0, MRR 1.0 and provenance
  precision 1.0; holdout recall@k 1.0 and provenance precision 1.0.
- Chronological baseline and context/embedding-disabled ablation are recorded
  separately from treatment metrics.
- Windows measurement: p50 10.55 ms, p95 11.40 ms, maximum 13.61 ms,
  operational memory 0.20 MiB and approximately 377 cases/second.
- Invariants, schema validation and deterministic replay passed.

## Gates executed

- C++ build and native CTest passed (22/22 in the current Windows build).
- Windows promotion validator passed: equivalence, schema, hashes, holdout,
  provenance, duplicate immutability, retention, ablation and performance.
- Python unit tests, targeted Ruff for SPEC-034 files, mypy, contracts,
  component maturity, configuration and documentation validation passed.

## Decision boundary

The component remains `reference_status: frozen`, `native_status: equivalent`
and `product_status: unavailable`. The promotion was not inserted into
`promotions/registry.json`; a human review with an `approval_review_id` is
required. Cross-language agreement and operational metrics are verification
evidence, not scientific or ecological validity.

Semantic consolidation, fact creation, summaries, models, sensors, actions,
network access and product tasks remain outside this SPEC.

## Artifacts

- `validation/reports/episodic_memory_v1.json`
- `validation/reports/episodic_memory_v1_divergences.json`
- `promotions/cognition.episodic_memory.v1.json`
- `validation/holdout/episodic_memory_manifest.json`
- `docs/03-contracts/EPISODIC_MEMORY_PROMOTION_CONTRACT.md`
