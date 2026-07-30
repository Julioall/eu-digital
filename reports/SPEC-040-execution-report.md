# SPEC-040 execution report

SPEC: SPEC-040 - Local CPU-first model and structured dialogue
Agent: Codex
Date: 2026-07-30

## Increment

- Added the backend-agnostic C++ `LocalModelBackend` port and optional
  `LocalModelGatewayPlugin`.
- Added one-heavy-model scheduling with priority and FIFO policies, timeout,
  cancellation forwarding, unload-after-request, idle-only backend swap and
  structured response validation.
- Added GGUF artifact validation for size, SHA-256, Portuguese compatibility,
  license compatibility, backend compatibility and separate runtime/payload
  identifiers.
- Added the explicit model-absent availability record and the local fixture
  runner `promotion_fixture_runner --local-model-dialogue`.
- Added Python reference equivalence, disjoint holdout, replay, baseline and
  unload/FIFO ablation evidence.

## Evidence and gates

- Native MinGW CTest: 22/22 passed, including the new gateway test.
- Native MSVC Debug target: build passed and `local_model_gateway` CTest passed.
- Python full suite: 198/198 tests passed with `PYTHONPATH=python`.
- Scoped Ruff and mypy: passed; mypy checked 29 source files.
- Contract, component-maturity and documentation validators: passed.
- Python/C++ development equivalence: passed with zero divergences.
- Three-case disjoint holdout: passed with zero divergences.
- Replay after latency normalization: passed.
- MinGW operational fixture: p50 10.85775 ms, p95 13.366 ms, max 13.366 ms,
  throughput 90882.61 cases/s.
- MSVC operational fixture: p50 14.83175 ms, p95 17.9764 ms, max 17.9764 ms,
  throughput 66686.07 cases/s.
- The memory probe is 0 MB because the validator uses a synthetic fixture and
  does not load a production model. The policy gate is 4 GiB payload and 7 GiB
  declared RAM; no production GGUF is selected by this SPEC.

## Scientific boundary and open decision

These results are computational verification of contracts, implementation
equivalence, holdout behavior and operational limits. They are not evidence of
cognition, dialogue quality, consciousness, emotion or intention.

The `detached_manifest_digest_v1` value binds the payload digest and separate
artifact IDs for local integrity testing. It is not asymmetric release
authentication. ADR-0015 also prohibits selecting a concrete inference runtime
or production GGUF in this boundary. A future ADR/SPEC must decide the runtime,
final license, release key and signature mechanism before product enablement.
The component remains `product_status: unavailable`.

## Reproducibility

- Promotion manifest: `promotions/inference.local_model_gateway.v1.json`
- Development fixtures: `validation/equivalence/local_model_dialogue_v1.jsonl`
- Holdout fixtures: `validation/holdout/local_model_dialogue_v1.jsonl`
- MinGW report: `validation/reports/local_model_dialogue_v1.json`
- MSVC report: `validation/reports/local_model_dialogue_v1_msvc.json`
