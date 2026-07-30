"""Deterministic discrete world-model and prediction-error reference.

The implementation is a local research primitive. It predicts observed state
transitions, records uncertainty and exposes a bounded surprise signal for the
workspace; it does not plan or execute actions.
"""

from __future__ import annotations

import math
import uuid
from collections import Counter, defaultdict, deque
from collections.abc import Mapping, Sequence
from dataclasses import dataclass, replace
from datetime import datetime
from enum import Enum
from typing import Any

from .schema_validation import validate_shared_schema

SCHEMA_VERSION = "1.0"
FREQUENCY_BASELINE_ID = "frequency_baseline_v0"
MARKOV_BASELINE_ID = "markov_order1_v0"
PREDICTOR_POLICY_ID = "incremental_markov_v1"
CREATED_BY = "world_model.incremental_markov.v1"
HYPOTHESIS = "explicit next-state prediction improves novelty detection"
ABLATION = "replace incremental context with frequency_baseline_v0"
FALSIFICATION = "prediction does not beat frequency on the frozen holdout"
_NAMESPACE = uuid.UUID("c0e8d9f3-0e10-4e15-93d6-4aa1d73dba1e")


class WorldModelError(ValueError):
    """Raised for invalid states, predictions or model configuration."""


class ModelPolicy(str, Enum):
    frequency = FREQUENCY_BASELINE_ID
    markov = MARKOV_BASELINE_ID
    incremental = PREDICTOR_POLICY_ID


@dataclass(frozen=True)
class PredictionConfig:
    max_order: int = 2
    smoothing: float = 1.0
    drift_window: int = 4
    drift_threshold: float = 1.5
    top_k: int = 3

    def __post_init__(self) -> None:
        if self.max_order < 1:
            raise WorldModelError("max_order must be positive")
        if not math.isfinite(self.smoothing) or self.smoothing <= 0:
            raise WorldModelError("smoothing must be finite and positive")
        if self.drift_window < 1:
            raise WorldModelError("drift_window must be positive")
        if not math.isfinite(self.drift_threshold) or self.drift_threshold < 0:
            raise WorldModelError("drift_threshold must be finite and non-negative")
        if self.top_k < 1:
            raise WorldModelError("top_k must be positive")


@dataclass(frozen=True)
class PromotedPatternInput:
    """Symbolic vocabulary received from the separately promoted pattern learner."""

    pattern_id: str
    status: str = "promoted"
    confidence: float = 1.0

    def __post_init__(self) -> None:
        if not self.pattern_id.strip():
            raise WorldModelError("pattern_id cannot be empty")
        if self.status != "promoted":
            raise WorldModelError("world model accepts only promoted patterns")
        if not math.isfinite(self.confidence) or not 0.0 <= self.confidence <= 1.0:
            raise WorldModelError("pattern confidence must be between zero and one")


@dataclass(frozen=True)
class Prediction:
    prediction_id: str
    schema_version: str
    model_id: str
    stream_id: str
    context: tuple[str, ...]
    predicted_distribution: dict[str, float]
    predicted_at: str
    top_k: int
    observed_state: str | None = None
    log_loss: float | None = None
    top_k_hit: bool | None = None
    salience_contribution: float = 0.0
    confidence: float = 1.0
    drift_id: str | None = None
    created_by: str = CREATED_BY

    def to_mapping(self) -> dict[str, Any]:
        value = {
            "prediction_id": self.prediction_id,
            "schema_version": self.schema_version,
            "model_id": self.model_id,
            "stream_id": self.stream_id,
            "context": list(self.context),
            "predicted_distribution": dict(self.predicted_distribution),
            "predicted_at": self.predicted_at,
            "top_k": self.top_k,
            "observed_state": self.observed_state,
            "log_loss": self.log_loss,
            "top_k_hit": self.top_k_hit,
            "salience_contribution": self.salience_contribution,
            "confidence": self.confidence,
            "drift_id": self.drift_id,
            "created_by": self.created_by,
        }
        validate_shared_schema(value, "world_model_prediction.schema.json")
        return value

    def error_mapping(self, observed_at: str) -> dict[str, Any]:
        if self.observed_state is None or self.log_loss is None or self.top_k_hit is None:
            raise WorldModelError("prediction has not been scored")
        _parse_time(observed_at)
        value = {
            "prediction_id": self.prediction_id,
            "schema_version": self.schema_version,
            "model_id": self.model_id,
            "observed_state": self.observed_state,
            "log_loss": self.log_loss,
            "top_k_hit": self.top_k_hit,
            "salience_contribution": self.salience_contribution,
            "confidence": self.confidence,
            "drift_id": self.drift_id,
            "observed_at": observed_at,
        }
        validate_shared_schema(value, "prediction_error.schema.json")
        return value


@dataclass(frozen=True)
class DriftSignal:
    drift_id: str
    schema_version: str
    model_id: str
    stream_id: str
    detected_at: str
    rolling_log_loss: float
    threshold: float
    confidence_before: float
    confidence_after: float
    relearning_started: bool
    trigger_prediction_id: str
    reason: str

    def to_mapping(self) -> dict[str, Any]:
        value = {
            "drift_id": self.drift_id,
            "schema_version": self.schema_version,
            "model_id": self.model_id,
            "stream_id": self.stream_id,
            "detected_at": self.detected_at,
            "rolling_log_loss": self.rolling_log_loss,
            "threshold": self.threshold,
            "confidence_before": self.confidence_before,
            "confidence_after": self.confidence_after,
            "relearning_started": self.relearning_started,
            "trigger_prediction_id": self.trigger_prediction_id,
            "reason": self.reason,
        }
        validate_shared_schema(value, "prediction_drift.schema.json")
        return value


class WorldModel:
    """Incremental finite-state predictor with explicit baselines."""

    def __init__(
        self,
        *,
        stream_id: str,
        policy: ModelPolicy | str = ModelPolicy.incremental,
        config: PredictionConfig | None = None,
        promoted_patterns: Sequence[Mapping[str, Any] | PromotedPatternInput] = (),
    ) -> None:
        if not stream_id.strip():
            raise WorldModelError("stream_id cannot be empty")
        try:
            self.policy = ModelPolicy(policy)
        except ValueError as error:
            raise WorldModelError(f"unsupported policy: {policy}") from error
        self.stream_id = stream_id
        self.config = config or PredictionConfig()
        self._promoted_patterns = self._normalize_promoted_patterns(promoted_patterns)
        self._global_counts: Counter[str] = Counter()
        self._transition_counts: defaultdict[tuple[str, ...], Counter[str]] = defaultdict(Counter)
        self._states: set[str] = set()
        self._history: list[str] = []
        self._predictions: dict[str, Prediction] = {}
        self._errors: list[float] = []
        self._rolling_errors: deque[float] = deque(maxlen=self.config.drift_window)
        self._drifts: list[DriftSignal] = []
        self._confidence = 1.0
        self._drift_latched = False
        self._relearning_started = False
        self._relearning_observations = 0
        self._sequence = 0

    @property
    def model_id(self) -> str:
        return self.policy.value

    def observe(self, state: str, event_ref: str, occurred_at: str) -> None:
        _required_state(state, "state")
        if not event_ref.strip():
            raise WorldModelError("event_ref cannot be empty")
        _parse_time(occurred_at)
        previous_history = tuple(self._history)
        self._states.add(state)
        self._global_counts[state] += 1
        if self.policy != ModelPolicy.frequency:
            max_order = 1 if self.policy == ModelPolicy.markov else self.config.max_order
            for order in range(1, min(max_order, len(previous_history)) + 1):
                context = previous_history[-order:]
                self._transition_counts[context][state] += 1
        self._history.append(state)
        if self._relearning_started:
            self._relearning_observations += 1

    def predict(
        self,
        context: Sequence[str] = (),
        *,
        predicted_at: str,
        candidate_states: Sequence[str] = (),
    ) -> Prediction:
        _parse_time(predicted_at)
        normalized_context = _normalize_context(context)
        states = self._known_states(candidate_states)
        if not states:
            raise WorldModelError("at least one observed or candidate state is required")
        distribution = self._distribution(normalized_context, states)
        self._sequence += 1
        prediction_id = str(uuid.uuid5(_NAMESPACE, f"{self.stream_id}:{self.model_id}:{self._sequence}"))
        prediction = Prediction(
            prediction_id=prediction_id,
            schema_version=SCHEMA_VERSION,
            model_id=self.model_id,
            stream_id=self.stream_id,
            context=normalized_context,
            predicted_distribution=distribution,
            predicted_at=predicted_at,
            top_k=min(self.config.top_k, len(states)),
            confidence=self._confidence,
        )
        prediction.to_mapping()
        self._predictions[prediction_id] = prediction
        return prediction

    def score(self, prediction: Prediction, observed_state: str, observed_at: str) -> Prediction:
        _required_state(observed_state, "observed_state")
        _parse_time(observed_at)
        stored = self._predictions.get(prediction.prediction_id)
        if stored is None or stored != prediction:
            raise WorldModelError("prediction does not belong to this model")
        if prediction.log_loss is not None:
            raise WorldModelError("prediction has already been scored")
        probability = prediction.predicted_distribution.get(observed_state, 0.0)
        log_loss = -math.log(max(probability, 1e-12))
        top_states = sorted(
            prediction.predicted_distribution,
            key=lambda state: (-prediction.predicted_distribution[state], state),
        )[: prediction.top_k]
        scored = replace(
            prediction,
            observed_state=observed_state,
            log_loss=log_loss,
            top_k_hit=observed_state in top_states,
            salience_contribution=prediction_error_to_salience(log_loss),
        )
        self._errors.append(log_loss)
        self._rolling_errors.append(log_loss)
        drift = self._detect_drift(scored, observed_at)
        if drift is not None:
            scored = replace(
                scored,
                confidence=drift.confidence_after,
                drift_id=drift.drift_id,
                salience_contribution=max(scored.salience_contribution, 0.9),
            )
        else:
            scored = replace(scored, confidence=self._confidence)
        scored.to_mapping()
        self._predictions[prediction.prediction_id] = scored
        return scored

    def latest_drift(self) -> DriftSignal | None:
        return self._drifts[-1] if self._drifts else None

    def metrics(self) -> dict[str, Any]:
        return {
            "model_id": self.model_id,
            "stream_id": self.stream_id,
            "prediction_count": len(self._predictions),
            "scored_count": len(self._errors),
            "mean_log_loss": sum(self._errors) / len(self._errors) if self._errors else None,
            "top_k_accuracy": self._top_k_accuracy(),
            "drift_count": len(self._drifts),
            "confidence": self._confidence,
            "relearning_started": self._relearning_started,
            "relearning_observations": self._relearning_observations,
            "promoted_pattern_count": len(self._promoted_patterns),
            "ablation": ABLATION,
            "falsification": FALSIFICATION,
        }

    def _known_states(self, candidate_states: Sequence[str]) -> list[str]:
        states = set(self._states) | set(self._promoted_patterns)
        for state in candidate_states:
            _required_state(state, "candidate state")
            states.add(state)
        return sorted(states)

    @staticmethod
    def _normalize_promoted_patterns(
        patterns: Sequence[Mapping[str, Any] | PromotedPatternInput],
    ) -> dict[str, PromotedPatternInput]:
        normalized: dict[str, PromotedPatternInput] = {}
        for value in patterns:
            if isinstance(value, PromotedPatternInput):
                pattern = value
            elif isinstance(value, Mapping):
                pattern = PromotedPatternInput(
                    pattern_id=str(value.get("pattern_id", "")),
                    status=str(value.get("status", "")),
                    confidence=float(value.get("confidence", 0.0)),
                )
            else:
                raise WorldModelError("promoted_patterns must contain mappings")
            if pattern.pattern_id in normalized:
                raise WorldModelError(f"duplicate promoted pattern: {pattern.pattern_id}")
            normalized[pattern.pattern_id] = pattern
        return normalized

    def _distribution(self, context: tuple[str, ...], states: list[str]) -> dict[str, float]:
        counts: Counter[str] | None = None
        if self.policy == ModelPolicy.frequency:
            counts = self._global_counts
        else:
            max_order = 1 if self.policy == ModelPolicy.markov else self.config.max_order
            bounded_context = context[-max_order:]
            for order in range(len(bounded_context), 0, -1):
                candidate = self._transition_counts.get(bounded_context[-order:])
                if candidate:
                    counts = candidate
                    break
            if counts is None:
                counts = self._global_counts
        denominator = sum(counts.get(state, 0) for state in states) + self.config.smoothing * len(states)
        return {
            state: (counts.get(state, 0) + self.config.smoothing) / denominator
            for state in states
        }

    def _detect_drift(self, prediction: Prediction, observed_at: str) -> DriftSignal | None:
        if len(self._rolling_errors) < self.config.drift_window:
            return None
        rolling_loss = sum(self._rolling_errors) / len(self._rolling_errors)
        if rolling_loss <= self.config.drift_threshold:
            self._drift_latched = False
            return None
        if self._drift_latched:
            return None
        before = self._confidence
        self._confidence = max(0.1, before * 0.5)
        self._transition_counts.clear()
        self._relearning_started = True
        self._drift_latched = True
        drift_id = str(uuid.uuid5(_NAMESPACE, f"drift:{self.stream_id}:{len(self._drifts) + 1}"))
        signal = DriftSignal(
            drift_id=drift_id,
            schema_version=SCHEMA_VERSION,
            model_id=self.model_id,
            stream_id=self.stream_id,
            detected_at=observed_at,
            rolling_log_loss=rolling_loss,
            threshold=self.config.drift_threshold,
            confidence_before=before,
            confidence_after=self._confidence,
            relearning_started=True,
            trigger_prediction_id=prediction.prediction_id,
            reason="rolling_prediction_error_exceeded_threshold",
        )
        signal.to_mapping()
        self._drifts.append(signal)
        return signal

    def _top_k_accuracy(self) -> float | None:
        scored = [prediction for prediction in self._predictions.values() if prediction.top_k_hit is not None]
        if not scored:
            return None
        return sum(bool(prediction.top_k_hit) for prediction in scored) / len(scored)


def prediction_error_to_salience(log_loss: float) -> float:
    """Map non-negative log loss monotonically to the workspace surprise factor."""

    if not math.isfinite(log_loss) or log_loss < 0:
        raise WorldModelError("log_loss must be finite and non-negative")
    return min(1.0, 1.0 - math.exp(-log_loss))


def _normalize_context(context: Sequence[str]) -> tuple[str, ...]:
    normalized = tuple(context)
    for state in normalized:
        _required_state(state, "context state")
    return normalized


def _required_state(value: str, field_name: str) -> None:
    if not isinstance(value, str) or not value.strip():
        raise WorldModelError(f"{field_name} must be a non-empty string")


def _parse_time(value: str) -> datetime:
    try:
        parsed = datetime.fromisoformat(value)
    except ValueError as error:
        raise WorldModelError(f"invalid timestamp: {value}") from error
    if parsed.tzinfo is None:
        raise WorldModelError("timestamps must include timezone")
    return parsed
