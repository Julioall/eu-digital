"""Capability plasticity and graceful-degradation reference for SPEC-024."""

from __future__ import annotations

import math
import uuid
from collections.abc import Mapping, Sequence
from dataclasses import dataclass
from datetime import datetime
from typing import Any

from .schema_validation import validate_shared_schema

SCHEMA_VERSION = "1.0"
BASELINE_POLICY_ID = "fixed_attention_v0"
ADAPTATION_POLICY_ID = "adaptive_observability_v1"
HYPOTHESIS = "adaptive observability preserves operation under capability change"
ABLATION = "keep fixed attention weights and confidence after capability removal"
FALSIFICATION = "adaptation does not outperform fixed attention under ablation"
_NAMESPACE = uuid.UUID("8f10f4ce-d53a-4ff2-974d-0c4c4d5c7895")
_UNAVAILABLE_STATES = frozenset({"unknown", "calibrating", "temporarily_unavailable", "disabled", "failed", "removed", "incompatible"})


class CapabilityAdaptationError(ValueError):
    """Raised for invalid capability changes or adaptation inputs."""


@dataclass(frozen=True)
class AdaptationEvent:
    adaptation_id: str
    schema_version: str
    agent_id: str
    capability_id: str
    implementation_id: str
    modality: str
    old_state: str
    new_state: str
    observed_at: str
    affected_belief_ids: tuple[str, ...]
    invalidated_prediction_ids: tuple[str, ...]
    blocked_plan_ids: tuple[str, ...]
    confidence_adjustments: dict[str, float]
    attention_weights: dict[str, float]
    limitation_code: str
    source_observation_present: bool

    def to_mapping(self) -> dict[str, Any]:
        value = {
            "adaptation_id": self.adaptation_id,
            "schema_version": self.schema_version,
            "agent_id": self.agent_id,
            "capability_id": self.capability_id,
            "implementation_id": self.implementation_id,
            "modality": self.modality,
            "old_state": self.old_state,
            "new_state": self.new_state,
            "observed_at": self.observed_at,
            "affected_belief_ids": list(self.affected_belief_ids),
            "invalidated_prediction_ids": list(self.invalidated_prediction_ids),
            "blocked_plan_ids": list(self.blocked_plan_ids),
            "confidence_adjustments": dict(self.confidence_adjustments),
            "attention_weights": dict(self.attention_weights),
            "limitation_code": self.limitation_code,
            "source_observation_present": self.source_observation_present,
        }
        validate_shared_schema(value, "capability_adaptation_event.schema.json")
        return value


@dataclass(frozen=True)
class OnboardingRecord:
    onboarding_id: str
    schema_version: str
    agent_id: str
    capability_id: str
    modality: str
    state: str
    calibration_samples: int
    required_samples: int
    stable_influence: bool
    observed_at: str

    def to_mapping(self) -> dict[str, Any]:
        value = {
            "onboarding_id": self.onboarding_id,
            "schema_version": self.schema_version,
            "agent_id": self.agent_id,
            "capability_id": self.capability_id,
            "modality": self.modality,
            "state": self.state,
            "calibration_samples": self.calibration_samples,
            "required_samples": self.required_samples,
            "stable_influence": self.stable_influence,
            "observed_at": self.observed_at,
        }
        validate_shared_schema(value, "capability_onboarding.schema.json")
        return value


class CapabilityAdaptationEngine:
    """Keep observability, confidence and plan compatibility explicit."""

    def __init__(self, *, agent_id: str, attention_weights: Mapping[str, float], calibration_samples: int = 3) -> None:
        _required_name(agent_id, "agent_id")
        if calibration_samples < 1:
            raise CapabilityAdaptationError("calibration_samples must be positive")
        self.agent_id = agent_id
        self.calibration_samples = calibration_samples
        self._base_weights = _normalize_weights(attention_weights)
        self._attention_weights: dict[str, float] = {}
        self._capabilities: dict[str, tuple[str, str, str]] = {}
        self._modality_capability: dict[str, str] = {}
        self._calibration_counts: dict[str, int] = {}
        self._beliefs: dict[str, tuple[tuple[str, ...], float, float]] = {}
        self._predictions: dict[str, tuple[str, ...]] = {}
        self._plans: dict[str, tuple[str, ...]] = {}
        self._blocked_plans: set[str] = set()
        self._history: list[str] = []
        self._events: list[AdaptationEvent] = []
        self._version = 0

    def register_capability(self, capability_id: str, modality: str, *, state: str = "unknown", implementation_id: str | None = None) -> None:
        _required_name(capability_id, "capability_id")
        _required_name(modality, "modality")
        _required_name(state, "state")
        implementation = implementation_id or capability_id
        if modality not in self._base_weights:
            self._base_weights = _normalize_weights({**self._base_weights, modality: 1.0})
        self._capabilities[capability_id] = (implementation, modality, state)
        self._modality_capability[modality] = capability_id
        self._history.append(implementation)
        self._recompute()

    def register_belief(self, belief_id: str, required_modalities: Sequence[str], confidence: float) -> None:
        _required_name(belief_id, "belief_id")
        modalities = _normalize_names(required_modalities, "required_modalities")
        if not 0 <= confidence <= 1 or not math.isfinite(confidence):
            raise CapabilityAdaptationError("confidence must be between zero and one")
        self._beliefs[belief_id] = (modalities, float(confidence), float(confidence))
        self._recompute()

    def register_prediction(self, prediction_id: str, required_modalities: Sequence[str]) -> None:
        _required_name(prediction_id, "prediction_id")
        self._predictions[prediction_id] = _normalize_names(required_modalities, "required_modalities")

    def register_plan(self, plan_id: str, required_capability_ids: Sequence[str]) -> None:
        _required_name(plan_id, "plan_id")
        self._plans[plan_id] = _normalize_names(required_capability_ids, "required_capability_ids")
        self._refresh_blocked_plans()

    def apply_change(
        self,
        *,
        capability_id: str,
        old_state: str,
        new_state: str,
        observed_at: str,
    ) -> AdaptationEvent:
        _parse_time(observed_at)
        if capability_id not in self._capabilities:
            raise CapabilityAdaptationError(f"unknown capability: {capability_id}")
        implementation_id, modality, current_state = self._capabilities[capability_id]
        if old_state != current_state:
            raise CapabilityAdaptationError(f"expected state {current_state!r}, got {old_state!r}")
        _required_name(new_state, "new_state")
        self._capabilities[capability_id] = (implementation_id, modality, new_state)
        if implementation_id not in self._history:
            self._history.append(implementation_id)
        affected = tuple(sorted(belief_id for belief_id, (modalities, _, _) in self._beliefs.items() if modality in modalities))
        invalidated = tuple(sorted(prediction_id for prediction_id, modalities in self._predictions.items() if modality in modalities and new_state in _UNAVAILABLE_STATES))
        self._refresh_blocked_plans()
        self._recompute()
        self._version += 1
        limitation = "capability_available" if new_state == "available" else "modality_calibrating" if new_state == "calibrating" else "modality_unavailable"
        event = AdaptationEvent(
            adaptation_id=str(uuid.uuid5(_NAMESPACE, f"{self.agent_id}:{capability_id}:{self._version}")),
            schema_version=SCHEMA_VERSION,
            agent_id=self.agent_id,
            capability_id=capability_id,
            implementation_id=implementation_id,
            modality=modality,
            old_state=old_state,
            new_state=new_state,
            observed_at=observed_at,
            affected_belief_ids=affected,
            invalidated_prediction_ids=invalidated,
            blocked_plan_ids=tuple(sorted(self._blocked_plans)),
            confidence_adjustments={belief_id: self._beliefs[belief_id][2] for belief_id in affected},
            attention_weights=dict(self._attention_weights),
            limitation_code=limitation,
            source_observation_present=True,
        )
        event.to_mapping()
        self._events.append(event)
        return event

    def calibrate(self, capability_id: str, samples: int, observed_at: str) -> OnboardingRecord:
        if samples < 1:
            raise CapabilityAdaptationError("calibration samples must be positive")
        _parse_time(observed_at)
        if capability_id not in self._capabilities:
            raise CapabilityAdaptationError(f"unknown capability: {capability_id}")
        _implementation_id, modality, state = self._capabilities[capability_id]
        if state != "calibrating":
            raise CapabilityAdaptationError("capability must be calibrating")
        count = self._calibration_counts.get(capability_id, 0) + samples
        self._calibration_counts[capability_id] = count
        if count >= self.calibration_samples:
            self.apply_change(capability_id=capability_id, old_state="calibrating", new_state="available", observed_at=observed_at)
            state = "available"
        record = OnboardingRecord(
            onboarding_id=str(uuid.uuid5(_NAMESPACE, f"onboarding:{self.agent_id}:{capability_id}:{count}")),
            schema_version=SCHEMA_VERSION,
            agent_id=self.agent_id,
            capability_id=capability_id,
            modality=modality,
            state=state,
            calibration_samples=count,
            required_samples=self.calibration_samples,
            stable_influence=state == "available",
            observed_at=observed_at,
        )
        record.to_mapping()
        return record

    def profile(self) -> dict[str, Any]:
        available = sorted(modality for _, modality, state in self._capabilities.values() if state == "available")
        calibrating = sorted(modality for _, modality, state in self._capabilities.values() if state == "calibrating")
        unavailable = sorted(set(self._base_weights) - set(available) - set(calibrating))
        value = {
            "profile_id": f"{self.agent_id}:observability:{self._version}",
            "schema_version": SCHEMA_VERSION,
            "agent_id": self.agent_id,
            "version": max(1, self._version),
            "available_modalities": available,
            "unavailable_modalities": unavailable,
            "calibrating_modalities": calibrating,
            "attention_weights": dict(self._attention_weights),
            "confidence_adjustments": {belief_id: confidence for belief_id, (_, _, confidence) in sorted(self._beliefs.items())},
            "limitation_codes": sorted(self._limitation_codes()),
            "capability_history": sorted(set(self._history)),
            "identity_generation": 1,
        }
        validate_shared_schema(value, "observability_profile.schema.json")
        return value

    def metrics(self) -> dict[str, Any]:
        available = set(self.profile()["available_modalities"])
        adaptive_attention = sum(self._attention_weights.get(modality, 0.0) for modality in available)
        fixed_attention = sum(self._base_weights.get(modality, 0.0) for modality in available)
        return {
            "policy_id": ADAPTATION_POLICY_ID,
            "baseline_id": BASELINE_POLICY_ID,
            "adaptive_observable_attention": adaptive_attention,
            "fixed_observable_attention": fixed_attention,
            "attention_gain": adaptive_attention - fixed_attention,
            "blocked_plan_count": len(self._blocked_plans),
            "adaptation_event_count": len(self._events),
            "ablation": ABLATION,
            "falsification": FALSIFICATION,
        }

    def events(self) -> tuple[AdaptationEvent, ...]:
        return tuple(self._events)

    def _refresh_blocked_plans(self) -> None:
        self._blocked_plans = {
            plan_id
            for plan_id, required in self._plans.items()
            if any(self._capabilities.get(capability_id, ("", "", "unknown"))[2] != "available" for capability_id in required)
        }

    def _recompute(self) -> None:
        available = {modality for _, modality, state in self._capabilities.values() if state == "available"}
        total = sum(self._base_weights.get(modality, 0.0) for modality in available)
        self._attention_weights = {modality: self._base_weights[modality] / total for modality in sorted(available) if total > 0}
        for belief_id, (modalities, base, _) in self._beliefs.items():
            missing = set(modalities) - available
            factor = 0.4 if missing and missing == set(modalities) else 0.7 if missing else 1.0
            self._beliefs[belief_id] = (modalities, base, base * factor)

    def _limitation_codes(self) -> set[str]:
        codes = set()
        if any(state in _UNAVAILABLE_STATES - {"unknown"} for _, _, state in self._capabilities.values()):
            codes.add("partial_observability")
        if any(state == "calibrating" for _, _, state in self._capabilities.values()):
            codes.add("calibration_pending")
        if self._blocked_plans:
            codes.add("plan_capability_missing")
        return codes


def _normalize_weights(values: Mapping[str, float]) -> dict[str, float]:
    if not values:
        raise CapabilityAdaptationError("at least one attention weight is required")
    result: dict[str, float] = {}
    for name, value in values.items():
        _required_name(name, "modality")
        if isinstance(value, bool) or not math.isfinite(float(value)) or float(value) <= 0:
            raise CapabilityAdaptationError("attention weights must be positive finite numbers")
        result[name] = float(value)
    total = sum(result.values())
    return {name: value / total for name, value in sorted(result.items())}


def _normalize_names(values: Sequence[str], field_name: str) -> tuple[str, ...]:
    names = tuple(sorted({_required_name(value, field_name) for value in values}))
    if not names:
        raise CapabilityAdaptationError(f"{field_name} cannot be empty")
    return names


def _required_name(value: str, field_name: str) -> str:
    if not isinstance(value, str) or not value.strip():
        raise CapabilityAdaptationError(f"{field_name} must be a non-empty string")
    return value.strip()


def _parse_time(value: str) -> datetime:
    try:
        parsed = datetime.fromisoformat(value)
    except ValueError as error:
        raise CapabilityAdaptationError(f"invalid timestamp: {value}") from error
    if parsed.tzinfo is None:
        raise CapabilityAdaptationError("timestamps must include timezone")
    return parsed
