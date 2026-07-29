# Laboratório Python

Este diretório contém protótipos, treinamento, análise, implementações de referência e experimentos.

Código experimental não entra automaticamente no Cérebro Implantado. A promoção segue a SPEC-026.

O sandbox reprodutível da SPEC-017 está em `eu_digital_lab/` e usa somente a
biblioteca padrão. Execute `python -m unittest discover -s python/tests -v` para
validar reprodutibilidade, ground truth, splits e anotações humanas.

Para validar o pacote isolado e a fixture compartilhada, use
`uv run python tools/validate_hybrid.py`.

The SPEC-002 in-process asynchronous bus is in `eu_digital_lab/event_bus.py`.
It validates `CanonicalEvent` with the shared schema, applies bounded-queue
backpressure, deduplicates `event_id`, and retains rejected events locally.
The Python-free C++ runtime equivalent is `cpp/core/event_bus.hpp`.

The SPEC-023 capability reference runtime is in
`eu_digital_lab/capabilities.py`. It loads local manifests and entry points,
validates shared contracts, tracks lifecycle and checkpoints, selects
providers by operation, supports explicit profiles and records changes in the
self-model. Plugins are ports; no concrete integration is imported by the
core.

The SPEC-018 evaluation harness is in `eu_digital_lab/evaluation.py`. It runs
baseline/treatment experiments behind feature flags, records commit/hardware/
backend/configuration provenance, keeps cognitive and operational metrics
separate, summarizes uncertainty, protects the manifest-backed holdout, and
provides deterministic virtual-clock replay, fault injection and metamorphic
checks. Its reports are engineering/scientific evidence records, not claims of
cognitive validity.

The SPEC-026 promotion pipeline is in `eu_digital_lab/promotion.py`. It freezes
canonical JSON-lines fixtures, sends identical bytes to Python and native
runners, persists semantic divergences, checks reviewed tolerance changes,
records performance evidence, emits reports, and maintains the approved
promotion registry. Agreement between runners is not ground truth.
The SPEC-027 validation gates are in `eu_digital_lab/validation.py`. They
require a frozen reviewed protocol, validate independent evidence, compare
backends and hardware, audit export/quantization, reproduce jitter and faults,
sequence controlled online sessions after replay, and produce longitudinal
reports with cognitive and operational metrics kept separate.
The SPEC-007 reference segmenter is in `eu_digital_lab/episode_segmentation.py`.
It implements the registered `time_context_threshold_v1` baseline with
explainable boundaries, shared Episode-schema output, deterministic offline
reprocessing, and boundary/WindowDiff metrics. It is laboratory code and is
not a promoted C++ cognitive mechanism.
The SPEC-008 episodic memory reference is in `eu_digital_lab/episodic_memory.py`.
It stores immutable Episode records locally, retrieves by observed context or
optional local embeddings, returns reason codes and event provenance, and
applies bounded retention without converting hypotheses into facts.
The SPEC-009 pattern learner reference is in `eu_digital_lab/pattern_learning.py`.
It incrementally clusters numeric observations, waits for configurable support
before promotion, records human feedback and concept-drift versions, and
publishes cluster metrics without executing actions.

The SPEC-010 global-workspace reference is in
`eu_digital_lab/global_workspace.py`. It ranks only explicitly observed
salience factors into a short-lived, bounded workspace; records selection and
exclusion reasons; expires entries; and broadcasts validated snapshots through
an injected local event-bus port. FIFO is selectable through the same
configuration for ablation. The module is laboratory-only and makes no claim
about consciousness or promotion to C++.

The SPEC-011 metacognition-and-curiosity reference is in
`eu_digital_lab/metacognition_curiosity.py`. It evaluates explicit hypotheses,
records confirmed/rejected/inconclusive local outcomes, calibrates confidence
against only verified outcomes, and emits structured question proposals with
information-gain estimates. Budget, cooldown, correction and redundancy rules
can suppress a proposal; it never sends a message, searches externally,
executes an action or invokes an LLM. The module is laboratory-only and its
calibration metrics are evidence records, not a claim of cognitive validity.

The SPEC-012 functional self-model reference is in
`eu_digital_lab/functional_self_model.py`. It turns typed internal updates into
immutable, hash-linked snapshots; keeps facts, hypotheses and configuration
separate; and emits explanation-backed structural capability decisions. The
`unconstrained_decision_v0` control is selected through the same interface for
ablation. It does not execute a decision, persist a longitudinal identity,
invoke an LLM, or import a concrete capability implementation.

The SPEC-013 local-model gateway reference is in
`eu_digital_lab/local_model_gateway.py`. It accepts an injected local backend,
serializes heavy inference through one worker, supports stable priority or FIFO
ablation, cancellation, timeout and unloading, and validates structured
`kind`/`fields` output. It does not select or download a model, import an
inference runtime, send data to an API, or perform dialogue or actions.

The SPEC-014 dialogue-and-avatar reference is in
`eu_digital_lab/dialogue_avatar.py`. It presents validated contextual notices,
keeps feedback history, and supports correction, defer and silence through an
injected presenter port. Its view state never blocks work, captures input or
claims emotion; no desktop framework is required by the laboratory reference.
