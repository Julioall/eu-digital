"""Deterministic laboratory reference for digital proprioception and agency."""

from __future__ import annotations

from collections.abc import Mapping, Sequence
from dataclasses import dataclass
from datetime import UTC, datetime
from enum import Enum
from typing import Any

from .schema_validation import validate_shared_schema

SCHEMA_VERSION = "1.0"
AGENCY_POLICY_ID = "agency_attribution_v1"
BASELINE_POLICY_ID = "passive_observer_v0"
HYPOTHESIS = (
    "linking reversible actions to predicted effects improves own/external "
    "attribution and reduces prediction error versus a passive observer"
)
ABLATION = "replace agency_attribution_v1 with passive_observer_v0"
FALSIFICATION = (
    "treatment does not beat baseline on frozen holdout, or treats absent "
    "correlation as evidence of an external effect"
)


class DigitalAgencyError(ValueError):
    """Raised when a proprioception or agency contract is invalid."""


class AgencyPolicy(str, Enum):
    agency_attribution_v1 = AGENCY_POLICY_ID
    passive_observer_v0 = BASELINE_POLICY_ID


class AttributionLabel(str, Enum):
    own = "own"
    external = "external"
    ambiguous = "ambiguous"


@dataclass(frozen=True)
class DigitalBodyState:
    state_id: str
    occurred_at: str
    active_capabilities: tuple[str, ...]
    queued_events: int
    available_actions: tuple[str, ...]
    initiated_actions: tuple[str, ...]
    failures: tuple[str, ...]
    limitations: tuple[str, ...]
    latencies_ms: Mapping[str, float]
    avatar_state: str
    action_origins: Mapping[str, str]
    schema_version: str = SCHEMA_VERSION

    def __post_init__(self) -> None:
        _required(self.state_id, "state_id")
        _time(self.occurred_at, "occurred_at")
        _strings(self.active_capabilities, "active_capabilities", allow_empty=True)
        _strings(self.available_actions, "available_actions", allow_empty=True)
        _strings(self.initiated_actions, "initiated_actions", allow_empty=True)
        _strings(self.failures, "failures", allow_empty=True)
        _strings(self.limitations, "limitations", allow_empty=True)
        if self.queued_events < 0:
            raise DigitalAgencyError("queued_events must be non-negative")
        if self.schema_version != SCHEMA_VERSION:
            raise DigitalAgencyError("unsupported body state schema version")
        if not self.avatar_state:
            raise DigitalAgencyError("avatar_state is required")
        if any(value < 0 for value in self.latencies_ms.values()):
            raise DigitalAgencyError("latencies must be non-negative")

    def to_mapping(self) -> dict[str, Any]:
        value = {
            "schema_version": self.schema_version,
            "state_id": self.state_id,
            "occurred_at": self.occurred_at,
            "active_capabilities": list(self.active_capabilities),
            "queued_events": self.queued_events,
            "available_actions": list(self.available_actions),
            "initiated_actions": list(self.initiated_actions),
            "failures": list(self.failures),
            "limitations": list(self.limitations),
            "latencies_ms": dict(self.latencies_ms),
            "avatar_state": self.avatar_state,
            "action_origins": dict(self.action_origins),
        }
        validate_shared_schema(value, "digital_body_state.schema.json")
        return value


@dataclass(frozen=True)
class ActionIntention:
    intention_id: str
    action_id: str
    operation: str
    target: str
    created_at: str
    predicted_effects: tuple[str, ...]
    confidence: float
    origin: str
    schema_version: str = SCHEMA_VERSION

    def __post_init__(self) -> None:
        for name, value in (
            ("intention_id", self.intention_id),
            ("action_id", self.action_id),
            ("operation", self.operation),
            ("target", self.target),
            ("origin", self.origin),
        ):
            _required(value, name)
        _time(self.created_at, "created_at")
        _strings(self.predicted_effects, "predicted_effects")
        _probability(self.confidence, "confidence")
        if self.schema_version != SCHEMA_VERSION:
            raise DigitalAgencyError("unsupported intention schema version")

    def to_mapping(self) -> dict[str, Any]:
        value = {
            "schema_version": self.schema_version,
            "intention_id": self.intention_id,
            "action_id": self.action_id,
            "operation": self.operation,
            "target": self.target,
            "created_at": self.created_at,
            "predicted_effects": list(self.predicted_effects),
            "confidence": self.confidence,
            "origin": self.origin,
        }
        validate_shared_schema(value, "action_intention.schema.json")
        return value


@dataclass(frozen=True)
class EfferenceCopy:
    copy_id: str
    action_id: str
    issued_at: str
    expected_effects: tuple[str, ...]
    observation_window_ms: int
    control_id: str
    schema_version: str = SCHEMA_VERSION

    def __post_init__(self) -> None:
        for name, value in (
            ("copy_id", self.copy_id),
            ("action_id", self.action_id),
            ("control_id", self.control_id),
        ):
            _required(value, name)
        _time(self.issued_at, "issued_at")
        _strings(self.expected_effects, "expected_effects")
        if self.observation_window_ms < 1:
            raise DigitalAgencyError("observation_window_ms must be positive")
        if self.schema_version != SCHEMA_VERSION:
            raise DigitalAgencyError("unsupported efference copy schema version")

    def to_mapping(self) -> dict[str, Any]:
        value = {
            "schema_version": self.schema_version,
            "copy_id": self.copy_id,
            "action_id": self.action_id,
            "issued_at": self.issued_at,
            "expected_effects": list(self.expected_effects),
            "observation_window_ms": self.observation_window_ms,
            "control_id": self.control_id,
        }
        validate_shared_schema(value, "efference_copy.schema.json")
        return value


@dataclass(frozen=True)
class AgencyActionOutcome:
    outcome_id: str
    action_id: str | None
    occurred_at: str
    observed_effects: tuple[str, ...]
    success: bool
    observation_origin: str
    correlation_id: str | None
    schema_version: str = SCHEMA_VERSION

    def __post_init__(self) -> None:
        _required(self.outcome_id, "outcome_id")
        if self.action_id is not None:
            _required(self.action_id, "action_id")
        _time(self.occurred_at, "occurred_at")
        _strings(self.observed_effects, "observed_effects")
        if self.observation_origin not in {"unclassified", "explicit_external"}:
            raise DigitalAgencyError("unsupported observation origin")
        if self.correlation_id is not None:
            _required(self.correlation_id, "correlation_id")
        if self.schema_version != SCHEMA_VERSION:
            raise DigitalAgencyError("unsupported outcome schema version")

    def to_mapping(self) -> dict[str, Any]:
        value = {
            "schema_version": self.schema_version,
            "outcome_id": self.outcome_id,
            "action_id": self.action_id,
            "occurred_at": self.occurred_at,
            "observed_effects": list(self.observed_effects),
            "success": self.success,
            "observation_origin": self.observation_origin,
            "correlation_id": self.correlation_id,
        }
        validate_shared_schema(value, "agency_action_outcome.schema.json")
        return value


@dataclass(frozen=True)
class AgencyAttribution:
    attribution_id: str
    action_id: str
    label: AttributionLabel
    confidence: float
    evidence: tuple[str, ...]
    prediction_error: float
    policy_id: str
    schema_version: str = SCHEMA_VERSION

    def __post_init__(self) -> None:
        _required(self.attribution_id, "attribution_id")
        _required(self.action_id, "action_id")
        try:
            object.__setattr__(self, "label", AttributionLabel(self.label))
        except ValueError as error:
            raise DigitalAgencyError("unsupported attribution label") from error
        _probability(self.confidence, "confidence")
        _strings(self.evidence, "evidence")
        if self.prediction_error < 0:
            raise DigitalAgencyError("prediction_error must be non-negative")
        _required(self.policy_id, "policy_id")
        if self.schema_version != SCHEMA_VERSION:
            raise DigitalAgencyError("unsupported attribution schema version")

    def to_mapping(self) -> dict[str, Any]:
        value = {
            "schema_version": self.schema_version,
            "attribution_id": self.attribution_id,
            "action_id": self.action_id,
            "label": self.label.value,
            "confidence": self.confidence,
            "evidence": list(self.evidence),
            "prediction_error": self.prediction_error,
            "policy_id": self.policy_id,
        }
        validate_shared_schema(value, "agency_attribution.schema.json")
        return value


class DigitalAgencyEngine:
    """Record the agency loop with an explicit passive-observer ablation."""

    def __init__(self, policy: AgencyPolicy = AgencyPolicy.agency_attribution_v1) -> None:
        try:
            self.policy = AgencyPolicy(policy)
        except ValueError as error:
            raise DigitalAgencyError("unsupported agency policy") from error
        self._intentions: dict[str, ActionIntention] = {}
        self._copies: dict[str, EfferenceCopy] = {}
        self._attributions: dict[str, AgencyAttribution] = {}
        self._body_states: list[DigitalBodyState] = []

    def record_body_state(self, state: DigitalBodyState) -> None:
        state.to_mapping()
        self._body_states.append(state)

    def start(self, intention: ActionIntention, *, control_id: str, observation_window_ms: int) -> EfferenceCopy | None:
        intention.to_mapping()
        if intention.action_id in self._intentions:
            raise DigitalAgencyError("action_id already registered")
        self._intentions[intention.action_id] = intention
        if self.policy is AgencyPolicy.passive_observer_v0:
            return None
        copy = EfferenceCopy(
            copy_id=f"copy-{intention.action_id}",
            action_id=intention.action_id,
            issued_at=intention.created_at,
            expected_effects=intention.predicted_effects,
            observation_window_ms=observation_window_ms,
            control_id=control_id,
        )
        self._copies[intention.action_id] = copy
        return copy

    def observe(self, outcome: AgencyActionOutcome) -> AgencyAttribution:
        outcome.to_mapping()
        action_id = outcome.action_id or f"unmatched-{outcome.outcome_id}"
        copy = self._copies.get(action_id)
        intention = self._intentions.get(action_id)
        if outcome.observation_origin == "explicit_external":
            label = AttributionLabel.external
            confidence = 0.9
            evidence: tuple[str, ...] = ("explicit_external_observation",)
        elif (
            self.policy is AgencyPolicy.agency_attribution_v1
            and intention is not None
            and copy is not None
            and outcome.action_id == action_id
            and outcome.correlation_id == copy.control_id
        ):
            label = AttributionLabel.own
            confidence = 0.95
            evidence = ("matching_action_id", "matching_control_id", "efference_copy")
        else:
            label = AttributionLabel.ambiguous
            confidence = 0.25
            evidence = ("insufficient_causal_correlation",)

        expected = copy.expected_effects if copy is not None else ()
        error = prediction_error(expected, outcome.observed_effects)
        attribution = AgencyAttribution(
            attribution_id=f"attribution-{outcome.outcome_id}",
            action_id=action_id,
            label=label,
            confidence=confidence,
            evidence=evidence,
            prediction_error=error,
            policy_id=self.policy.value,
        )
        attribution.to_mapping()
        self._attributions[action_id] = attribution
        return attribution

    @property
    def attributions(self) -> tuple[AgencyAttribution, ...]:
        return tuple(self._attributions.values())

    @property
    def body_states(self) -> tuple[DigitalBodyState, ...]:
        return tuple(self._body_states)

    def metrics(self, reference: Mapping[str, AttributionLabel | str]) -> dict[str, float | str]:
        predictions = {
            action_id: attribution.label
            for action_id, attribution in self._attributions.items()
            if action_id in reference
        }
        score = macro_f1(predictions, reference)
        error = (
            sum(item.prediction_error for item in self._attributions.values())
            / len(self._attributions)
            if self._attributions
            else 0.0
        )
        return {
            "policy_id": self.policy.value,
            "macro_f1": score,
            "mean_prediction_error": error,
        }

    @staticmethod
    def scientific_metadata() -> dict[str, str]:
        return {
            "hypothesis": HYPOTHESIS,
            "baseline_id": BASELINE_POLICY_ID,
            "ablation": ABLATION,
            "falsification": FALSIFICATION,
            "primary_metric": "macro_f1_attribution",
        }


def prediction_error(expected: Sequence[str], observed: Sequence[str]) -> float:
    if not expected:
        return 1.0
    overlap = len(set(expected) & set(observed))
    return 1.0 - overlap / max(len(set(expected)), len(set(observed)), 1)


def macro_f1(
    predictions: Mapping[str, AttributionLabel | str],
    reference: Mapping[str, AttributionLabel | str],
) -> float:
    labels = tuple(AttributionLabel)
    scores: list[float] = []
    for label in labels:
        true_positive = sum(
            1 for key, expected in reference.items() if expected == label and predictions.get(key) == label
        )
        predicted_positive = sum(1 for value in predictions.values() if value == label)
        actual_positive = sum(1 for value in reference.values() if value == label)
        precision = true_positive / predicted_positive if predicted_positive else 0.0
        recall = true_positive / actual_positive if actual_positive else 0.0
        scores.append(2 * precision * recall / (precision + recall) if precision + recall else 0.0)
    return sum(scores) / len(scores)


def _required(value: str | None, name: str) -> None:
    if not isinstance(value, str) or not value.strip():
        raise DigitalAgencyError(f"{name} is required")


def _strings(values: Sequence[str], name: str, *, allow_empty: bool = False) -> None:
    if (not allow_empty and not values) or any(
        not isinstance(value, str) or not value.strip() for value in values
    ):
        raise DigitalAgencyError(f"{name} must contain non-empty strings")


def _probability(value: float, name: str) -> None:
    if isinstance(value, bool) or not isinstance(value, (int, float)) or not 0 <= value <= 1:
        raise DigitalAgencyError(f"{name} must be between zero and one")


def _time(value: str, name: str) -> None:
    try:
        parsed = datetime.fromisoformat(value)
    except ValueError as error:
        raise DigitalAgencyError(f"{name} must be ISO-8601") from error
    if parsed.tzinfo is None or parsed.tzinfo.utcoffset(parsed) is None:
        raise DigitalAgencyError(f"{name} must include timezone")
    if parsed.tzinfo != UTC:
        parsed.astimezone(UTC)
