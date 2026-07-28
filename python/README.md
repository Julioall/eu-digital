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
