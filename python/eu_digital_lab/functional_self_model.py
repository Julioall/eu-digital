"""Versioned, local functional self-model reference for the research lab.

The model records explicit internal updates and makes only structural
capability decisions. It does not invoke language models, send messages, or
perform actions on behalf of an orchestrator.
"""

from __future__ import annotations

import hashlib
import json
import uuid
from collections.abc import Collection, Mapping
from dataclasses import dataclass
from datetime import UTC, datetime
from enum import Enum
from typing import Any

from .schema_validation import validate_shared_schema

SCHEMA_VERSION = "1.0"
SELF_MODEL_POLICY_ID = "self_model_gate_v1"
BASELINE_POLICY_ID = "unconstrained_decision_v0"
HYPOTHESIS = (
    "a versioned self-model gate improves capability attribution, limitation "
    "explanations, and compatible decision selection versus an unconstrained control"
)
ABLATION = (
    "select unconstrained_decision_v0 through the same interface and omit "
    "snapshot consultation during a decision"
)
FALSIFICATION = (
    "removing snapshot consultation does not change relevant decisions, or "
    "snapshots misrepresent declared capability availability"
)
_NAMESPACE = uuid.UUID("c70b62a7-d37b-4ee9-9a58-3d595147e353")


class FunctionalSelfModelError(ValueError):
    """Raised when a self-model contract or immutable-history transition fails."""


class AssertionClassification(str, Enum):
    fact = "fact"
    hypothesis = "hypothesis"
    configuration = "configuration"


class CapabilityStatus(str, Enum):
    available = "available"
    degraded = "degraded"
    unavailable = "unavailable"
    removed = "removed"


class DecisionPolicy(str, Enum):
    self_model_gate_v1 = SELF_MODEL_POLICY_ID
    unconstrained_decision_v0 = BASELINE_POLICY_ID


class SelfModelEventKind(str, Enum):
    capability_changed = "capability_changed"
    assertion_recorded = "assertion_recorded"


@dataclass(frozen=True)
class SelfModelAssertion:
    """An explicit fact, hypothesis, or configuration item with provenance."""

    assertion_id: str
    subject: str
    predicate: str
    value: str
    classification: AssertionClassification
    explanation: str
    source_event_ids: tuple[str, ...]

    def __post_init__(self) -> None:
        _required_string(self.assertion_id, "assertion_id")
        _required_string(self.subject, "subject")
        _required_string(self.predicate, "predicate")
        _required_string(self.value, "value")
        try:
            object.__setattr__(
                self, "classification", AssertionClassification(self.classification)
            )
        except ValueError as error:
            raise FunctionalSelfModelError("unsupported assertion classification") from error
        _required_string(self.explanation, "explanation")
        object.__setattr__(
            self, "source_event_ids", _references(self.source_event_ids, "source_event_ids")
        )

    def to_mapping(self) -> dict[str, Any]:
        return {
            "assertion_id": self.assertion_id,
            "subject": self.subject,
            "predicate": self.predicate,
            "value": self.value,
            "classification": self.classification.value,
            "explanation": self.explanation,
            "source_event_ids": list(self.source_event_ids),
        }

    def to_mapping_without_sources(self) -> dict[str, Any]:
        value = self.to_mapping()
        value.pop("source_event_ids")
        return value


@dataclass(frozen=True)
class CapabilityEntry:
    """A declared capability state; absence remains unverified, not negative."""

    capability_id: str
    status: CapabilityStatus
    explanation: str
    source_event_ids: tuple[str, ...]

    def __post_init__(self) -> None:
        _required_string(self.capability_id, "capability_id")
        try:
            object.__setattr__(self, "status", CapabilityStatus(self.status))
        except ValueError as error:
            raise FunctionalSelfModelError("unsupported capability status") from error
        _required_string(self.explanation, "capability_explanation")
        object.__setattr__(
            self, "source_event_ids", _references(self.source_event_ids, "source_event_ids")
        )

    def to_mapping(self) -> dict[str, Any]:
        return {
            "capability_id": self.capability_id,
            "status": self.status.value,
            "explanation": self.explanation,
            "source_event_ids": list(self.source_event_ids),
        }


@dataclass(frozen=True)
class SelfModelEvent:
    """A typed local update that creates one new immutable model version."""

    event_id: str
    occurred_at: str
    kind: SelfModelEventKind
    reason: str
    source_event_ids: tuple[str, ...]
    capability_id: str | None
    capability_status: CapabilityStatus | None
    capability_explanation: str | None
    assertion: SelfModelAssertion | None
    schema_version: str = SCHEMA_VERSION

    def __post_init__(self) -> None:
        _required_string(self.event_id, "event_id")
        _parse_time(self.occurred_at, "occurred_at")
        try:
            object.__setattr__(self, "kind", SelfModelEventKind(self.kind))
        except ValueError as error:
            raise FunctionalSelfModelError("unsupported internal event kind") from error
        _required_string(self.reason, "reason")
        object.__setattr__(
            self, "source_event_ids", _references(self.source_event_ids, "source_event_ids")
        )
        if self.schema_version != SCHEMA_VERSION:
            raise FunctionalSelfModelError("unsupported internal event schema version")
        if self.kind is SelfModelEventKind.capability_changed:
            _required_string(self.capability_id, "capability_id")
            if self.capability_status is None:
                raise FunctionalSelfModelError("capability_changed requires capability_status")
            try:
                object.__setattr__(
                    self, "capability_status", CapabilityStatus(self.capability_status)
                )
            except ValueError as error:
                raise FunctionalSelfModelError("unsupported capability status") from error
            _required_string(self.capability_explanation, "capability_explanation")
            if self.assertion is not None:
                raise FunctionalSelfModelError("capability_changed cannot include an assertion")
        else:
            if any(
                value is not None
                for value in (
                    self.capability_id,
                    self.capability_status,
                    self.capability_explanation,
                )
            ):
                raise FunctionalSelfModelError("assertion_recorded cannot include capability state")
            if self.assertion is None:
                raise FunctionalSelfModelError("assertion_recorded requires an assertion")
        self.to_mapping()

    def to_mapping(self) -> dict[str, Any]:
        if self.kind is SelfModelEventKind.capability_changed:
            if (
                self.capability_id is None
                or self.capability_status is None
                or self.capability_explanation is None
            ):
                raise FunctionalSelfModelError("capability_changed is incomplete")
            capability: dict[str, Any] | None = {
                "capability_id": self.capability_id,
                "status": self.capability_status.value,
                "explanation": self.capability_explanation,
            }
            assertion = None
        else:
            if self.assertion is None:
                raise FunctionalSelfModelError("assertion_recorded is incomplete")
            capability = None
            assertion = self.assertion.to_mapping_without_sources()
        value = {
            "event_id": self.event_id,
            "schema_version": self.schema_version,
            "occurred_at": self.occurred_at,
            "kind": self.kind.value,
            "reason": self.reason,
            "source_event_ids": list(self.source_event_ids),
            "capability": capability,
            "assertion": assertion,
        }
        _validate_contract(value, "self_model_internal_event.schema.json")
        return value


@dataclass(frozen=True)
class FunctionalSelfModelSnapshot:
    """An immutable state version linked to the prior version by a hash chain."""

    snapshot_id: str
    schema_version: str
    version: int
    prior_snapshot_id: str | None
    updated_at: str
    trigger_event_id: str | None
    capabilities: tuple[CapabilityEntry, ...]
    facts: tuple[SelfModelAssertion, ...]
    hypotheses: tuple[SelfModelAssertion, ...]
    configuration: tuple[SelfModelAssertion, ...]
    history_hash: str

    def to_mapping(self) -> dict[str, Any]:
        value = {
            "snapshot_id": self.snapshot_id,
            "schema_version": self.schema_version,
            "version": self.version,
            "prior_snapshot_id": self.prior_snapshot_id,
            "updated_at": self.updated_at,
            "trigger_event_id": self.trigger_event_id,
            "capabilities": [entry.to_mapping() for entry in self.capabilities],
            "facts": [assertion.to_mapping() for assertion in self.facts],
            "hypotheses": [assertion.to_mapping() for assertion in self.hypotheses],
            "configuration": [assertion.to_mapping() for assertion in self.configuration],
            "history_hash": self.history_hash,
        }
        _validate_contract(value, "functional_self_model_snapshot.schema.json")
        return value


@dataclass(frozen=True)
class SelfModelDecision:
    """An auditable decision only; executing it is outside this SPEC."""

    decision_id: str
    schema_version: str
    snapshot_id: str
    requested_capability_id: str
    allowed: bool
    reason_code: str
    explanation: str
    policy_id: str

    def to_mapping(self) -> dict[str, Any]:
        value = {
            "decision_id": self.decision_id,
            "schema_version": self.schema_version,
            "snapshot_id": self.snapshot_id,
            "requested_capability_id": self.requested_capability_id,
            "allowed": self.allowed,
            "reason_code": self.reason_code,
            "explanation": self.explanation,
            "policy_id": self.policy_id,
        }
        _validate_contract(value, "self_model_decision.schema.json")
        return value


class VersionedFunctionalSelfModel:
    """Apply internal events and expose version-backed decision explanations."""

    def __init__(
        self,
        *,
        model_id: str,
        initial_at: str,
        decision_policy: DecisionPolicy = DecisionPolicy.self_model_gate_v1,
    ) -> None:
        _required_string(model_id, "model_id")
        moment = _parse_time(initial_at, "initial_at")
        try:
            self.decision_policy = DecisionPolicy(decision_policy)
        except ValueError as error:
            raise FunctionalSelfModelError("unsupported decision policy") from error
        self._model_id = model_id
        self._seen_event_ids: set[str] = set()
        self._history = [
            self._make_snapshot(
                version=0,
                prior=None,
                updated_at=moment,
                trigger_event_id=None,
                capabilities=(),
                facts=(),
                hypotheses=(),
                configuration=(),
            )
        ]

    @property
    def current(self) -> FunctionalSelfModelSnapshot:
        return self._history[-1]

    def apply(self, event: SelfModelEvent) -> FunctionalSelfModelSnapshot:
        """Append exactly one event-derived immutable snapshot."""

        if event.event_id in self._seen_event_ids:
            raise FunctionalSelfModelError("internal event was already applied")
        moment = _parse_time(event.occurred_at, "occurred_at")
        if moment < _parse_time(self.current.updated_at, "current.updated_at"):
            raise FunctionalSelfModelError("internal events must be applied in timestamp order")
        capabilities = {entry.capability_id: entry for entry in self.current.capabilities}
        facts = list(self.current.facts)
        hypotheses = list(self.current.hypotheses)
        configuration = list(self.current.configuration)
        if event.kind is SelfModelEventKind.capability_changed:
            if (
                event.capability_id is None
                or event.capability_status is None
                or event.capability_explanation is None
            ):
                raise FunctionalSelfModelError("capability_changed is incomplete")
            capabilities[event.capability_id] = CapabilityEntry(
                capability_id=event.capability_id,
                status=event.capability_status,
                explanation=event.capability_explanation,
                source_event_ids=event.source_event_ids,
            )
        else:
            if event.assertion is None:
                raise FunctionalSelfModelError("assertion_recorded is incomplete")
            target = {
                AssertionClassification.fact: facts,
                AssertionClassification.hypothesis: hypotheses,
                AssertionClassification.configuration: configuration,
            }[event.assertion.classification]
            target.append(event.assertion)
        next_snapshot = self._make_snapshot(
            version=self.current.version + 1,
            prior=self.current,
            updated_at=moment,
            trigger_event_id=event.event_id,
            capabilities=tuple(sorted(capabilities.values(), key=lambda entry: entry.capability_id)),
            facts=tuple(sorted(facts, key=lambda assertion: assertion.assertion_id)),
            hypotheses=tuple(sorted(hypotheses, key=lambda assertion: assertion.assertion_id)),
            configuration=tuple(
                sorted(configuration, key=lambda assertion: assertion.assertion_id)
            ),
        )
        self._seen_event_ids.add(event.event_id)
        self._history.append(next_snapshot)
        return next_snapshot

    def version(self, version: int) -> FunctionalSelfModelSnapshot:
        if not isinstance(version, int) or isinstance(version, bool) or version < 0:
            raise FunctionalSelfModelError("version must be a non-negative integer")
        if version >= len(self._history):
            raise FunctionalSelfModelError("requested self-model version is unavailable")
        return self._history[version]

    def decide(self, requested_capability_id: str) -> SelfModelDecision:
        """Return a decision whose treatment policy reads the current snapshot."""

        _required_string(requested_capability_id, "requested_capability_id")
        if self.decision_policy is DecisionPolicy.unconstrained_decision_v0:
            allowed = True
            reason_code = "baseline_unconstrained"
            explanation = "Baseline does not consult the functional self-model."
        else:
            entry = next(
                (
                    value
                    for value in self.current.capabilities
                    if value.capability_id == requested_capability_id
                ),
                None,
            )
            allowed, reason_code, explanation = _decision_for_capability(
                requested_capability_id, entry
            )
        decision_id = str(
            uuid.uuid5(
                _NAMESPACE,
                f"{self.current.snapshot_id}:{requested_capability_id}:"
                f"{self.decision_policy.value}:{reason_code}",
            )
        )
        decision = SelfModelDecision(
            decision_id=decision_id,
            schema_version=SCHEMA_VERSION,
            snapshot_id=self.current.snapshot_id,
            requested_capability_id=requested_capability_id,
            allowed=allowed,
            reason_code=reason_code,
            explanation=explanation,
            policy_id=self.decision_policy.value,
        )
        decision.to_mapping()
        return decision

    def metrics(self) -> dict[str, Any]:
        """Register engineering evidence without claiming scientific validity."""

        return {
            "policy_id": self.decision_policy.value,
            "baseline_policy_id": BASELINE_POLICY_ID,
            "hypothesis": HYPOTHESIS,
            "ablation": ABLATION,
            "falsification": FALSIFICATION,
            "history_version_count": len(self._history),
            "applied_event_count": len(self._seen_event_ids),
        }

    def snapshot(self) -> dict[str, Any]:
        """Return all immutable versions in deterministic order for replay."""

        return {
            "schema_version": SCHEMA_VERSION,
            "model_id": self._model_id,
            "history": [item.to_mapping() for item in self._history],
            "metrics": self.metrics(),
        }

    def _make_snapshot(
        self,
        *,
        version: int,
        prior: FunctionalSelfModelSnapshot | None,
        updated_at: datetime,
        trigger_event_id: str | None,
        capabilities: tuple[CapabilityEntry, ...],
        facts: tuple[SelfModelAssertion, ...],
        hypotheses: tuple[SelfModelAssertion, ...],
        configuration: tuple[SelfModelAssertion, ...],
    ) -> FunctionalSelfModelSnapshot:
        payload = {
            "model_id": self._model_id,
            "version": version,
            "prior_snapshot_id": None if prior is None else prior.snapshot_id,
            "prior_history_hash": None if prior is None else prior.history_hash,
            "updated_at": _format_time(updated_at),
            "trigger_event_id": trigger_event_id,
            "capabilities": [entry.to_mapping() for entry in capabilities],
            "facts": [assertion.to_mapping() for assertion in facts],
            "hypotheses": [assertion.to_mapping() for assertion in hypotheses],
            "configuration": [assertion.to_mapping() for assertion in configuration],
        }
        history_hash = hashlib.sha256(_canonical_json(payload).encode("utf-8")).hexdigest()
        snapshot = FunctionalSelfModelSnapshot(
            snapshot_id=str(uuid.uuid5(_NAMESPACE, f"{self._model_id}:{history_hash}")),
            schema_version=SCHEMA_VERSION,
            version=version,
            prior_snapshot_id=None if prior is None else prior.snapshot_id,
            updated_at=_format_time(updated_at),
            trigger_event_id=trigger_event_id,
            capabilities=capabilities,
            facts=facts,
            hypotheses=hypotheses,
            configuration=configuration,
            history_hash=history_hash,
        )
        snapshot.to_mapping()
        return snapshot


def _decision_for_capability(
    capability_id: str, entry: CapabilityEntry | None
) -> tuple[bool, str, str]:
    if entry is None:
        return (
            False,
            "capability_unverified",
            f"Capability {capability_id} is not declared; availability is unverified.",
        )
    if entry.status is CapabilityStatus.available:
        return True, "capability_available", entry.explanation
    if entry.status is CapabilityStatus.degraded:
        return False, "capability_degraded", entry.explanation
    if entry.status is CapabilityStatus.unavailable:
        return False, "capability_unavailable", entry.explanation
    return False, "capability_removed", entry.explanation


def _validate_contract(value: Mapping[str, Any], schema_name: str) -> None:
    try:
        validate_shared_schema(value, schema_name)
    except ValueError as error:
        raise FunctionalSelfModelError(str(error)) from error


def _required_string(value: str | None, name: str) -> None:
    if not isinstance(value, str) or not value.strip():
        raise FunctionalSelfModelError(f"{name} must be a non-empty string")


def _references(values: Collection[str], name: str) -> tuple[str, ...]:
    if not isinstance(values, Collection) or isinstance(values, str):
        raise FunctionalSelfModelError(f"{name} must be a collection of strings")
    normalized = tuple(values)
    if any(not isinstance(value, str) or not value.strip() for value in normalized):
        raise FunctionalSelfModelError(f"{name} must contain non-empty strings")
    return normalized


def _parse_time(value: str, name: str) -> datetime:
    if not isinstance(value, str):
        raise FunctionalSelfModelError(f"{name} must be an ISO-8601 string")
    try:
        parsed = datetime.fromisoformat(value)
    except ValueError as error:
        raise FunctionalSelfModelError(f"{name} must be a valid ISO-8601 timestamp") from error
    if parsed.tzinfo is None:
        raise FunctionalSelfModelError(f"{name} must include a timezone")
    return parsed.astimezone(UTC)


def _format_time(value: datetime) -> str:
    return value.astimezone(UTC).isoformat()


def _canonical_json(value: Mapping[str, Any]) -> str:
    return json.dumps(value, sort_keys=True, separators=(",", ":"), ensure_ascii=False)
