---
id: SPEC-040
title: Local CPU-first model and structured dialogue
status: done
phase: beta
dependencies: [SPEC-013, SPEC-014, SPEC-039]
adrs: [ADR-0003, ADR-0004, ADR-0009, ADR-0010, ADR-0011]
contracts: [LOCAL_MODEL_GATEWAY_SCHEMA.md, local_model_request.schema.json, local_model_response.schema.json, model_prompt_template.schema.json]
---

# SPEC-040 - Local CPU-first model and structured dialogue

## Objetivo

Integrate an optional C++ local CPU-first boundary with one heavy-model queue,
timeout, cancellation, unload, schemas and explicit degradation.

## Escopo negativo

No external API, network, rule-based semantic fallback, mandatory model startup,
or LLM as the cognitive core.

## Scope

The increment includes the GGUF artifact policy, license and Portuguese
compatibility checks, structured output, hash and compatibility validation,
separate runtime/payload identifiers, and minimal textual dialogue through an
injected backend port.

## Scientific and operational protocol

Operational hypothesis: a local backend within declared limits preserves the
single-heavy-model resource bound. Baseline: explicit model absence. Metrics:
schema validity, p50/p95 latency, memory limit, cancellation and queue state.
Ablations: model absence, FIFO scheduling and unload between requests.
Falsification: incompatibility, retained resource after timeout, invalid output,
or a second concurrent heavy inference.

## Critérios de aceite

- [x] GGUF artifacts are rejected above 4 GiB, require Portuguese-compatible
      language and compatible license, and the declared RAM limit is 7 GiB.
      The fixture uses a synthetic 1 MiB payload; no production model is
      selected by this SPEC.
- [x] Hash, compatibility, schema, timeout, cancellation and unload pass in
      native tests and the Python-to-C++ promotion runner.
- [x] Without a model, timeline, privacy and diagnostics remain available while
      dialogue-dependent functionality is disabled.
- [x] Runtime and payload use separate artifact identifiers bound by a detached
      manifest-digest integrity envelope. Asymmetric release signing remains a
      future release concern and is not claimed by this SPEC.
- [x] No external API or semantic fallback is introduced.

## Output

Optional local backend and structured textual dialogue; avatar and orchestrated
suggestions remain out of scope.

## Evidence

The native port and fixture backend are in
`cpp/core/local_model_gateway.hpp` and
`cpp/app/promotion_fixture_runner.cpp`. Reproducible equivalence, holdout,
replay and operational gates are recorded in
`validation/reports/local_model_dialogue_v1.json`.

This promotion freezes the removable gateway boundary and artifact policy. It
does not select a concrete GGUF, inference runtime or release signer: ADR-0015
requires those choices to receive their own ADR/SPEC. See
`docs/05-governance/OPEN_QUESTIONS.md`.
