"""Bounded, auditable global-workspace reference for the local research lab.

This is a deterministic selection mechanism, not an assertion of consciousness
or a promoted runtime mechanism. It only ranks explicitly observed signals;
missing observations stay visible and never become negative evidence.
"""

from __future__ import annotations

import hashlib
import json
import math
import uuid
from collections.abc import Awaitable, Callable, Collection, Mapping, Sequence
from dataclasses import dataclass, field, replace
from datetime import UTC, datetime, timedelta
from typing import Any

from .schema_validation import validate_shared_schema

SCHEMA_VERSION = "1.0"
POLICY_ID = "observed_weighted_mean_v1"
BASELINE_ID = "fifo_capacity_v0"
CREATED_BY = "global_workspace.observed_weighted_mean.v1"
HYPOTHESIS = (
    "a bounded workspace using observed salience signals improves annotated "
    "selection quality and noise robustness over FIFO capacity control"
)
ABLATION = "remove selected salience factors or replace the policy with fifo_capacity_v0"
FALSIFICATION = (
    "selection quality does not exceed FIFO on the frozen holdout, or capacity "
    "and broadcast removal do not change the relevant measures"
)

SALIENT_FACTORS = (
    "novelty",
    "surprise",
    "repetition",
    "conflict",
    "direct_mention",
    "goal_relevance",
    "risk",
    "learning_opportunity",
    "ignore_cost",
    "priority",
)
DEFAULT_WEIGHTS = {
    "novelty": 1.0,
    "surprise": 1.0,
    "repetition": 0.5,
    "conflict": 1.2,
    "direct_mention": 1.2,
    "goal_relevance": 1.1,
    "risk": 1.3,
    "learning_opportunity": 0.8,
    "ignore_cost": 1.0,
    "priority": 1.3,
}
SOURCE_KINDS = frozenset({"canonical_event", "episode", "pattern", "internal"})
SELECTION_POLICIES = frozenset({POLICY_ID, BASELINE_ID})
_NAMESPACE = uuid.UUID("f835ced2-e6e2-4d16-a414-e5bd3c931c86")

BroadcastPublisher = Callable[[Mapping[str, Any]], Awaitable[Any]]


class WorkspaceError(ValueError):
    """Raised when workspace inputs or lifecycle transitions are invalid."""


@dataclass(frozen=True)
class WorkspaceCandidate:
    """Generic candidate with provenance and explicitly observed salience signals."""

    candidate_id: str
    session_id: str
    source_kind: str
    source_refs: tuple[str, ...]
    observed_at: str
    content: Mapping[str, Any]
    salience_signals: Mapping[str, float]
    schema_version: str = SCHEMA_VERSION

    def __post_init__(self) -> None:
        _required_name(self.candidate_id, "candidate_id")
        _required_name(self.session_id, "session_id")
        if self.schema_version != SCHEMA_VERSION:
            raise WorkspaceError("unsupported workspace candidate schema version")
        if self.source_kind not in SOURCE_KINDS:
            raise WorkspaceError("source_kind is not supported by the workspace contract")
        if not self.source_refs or any(not isinstance(value, str) or not value.strip() for value in self.source_refs):
            raise WorkspaceError("source_refs must contain at least one non-empty reference")
        _parse_time(self.observed_at, "observed_at")
        object.__setattr__(self, "content", _copy_json_object(self.content, "content"))
        object.__setattr__(self, "source_refs", tuple(self.source_refs))
        object.__setattr__(self, "salience_signals", _normalize_signals(self.salience_signals))
        self.to_mapping()

    def to_mapping(self) -> dict[str, Any]:
        value = {
            "candidate_id": self.candidate_id,
            "schema_version": self.schema_version,
            "session_id": self.session_id,
            "source_kind": self.source_kind,
            "source_refs": list(self.source_refs),
            "observed_at": self.observed_at,
            "content": _copy_json_object(self.content, "content"),
            "salience_signals": dict(self.salience_signals),
        }
        try:
            validate_shared_schema(value, "workspace_candidate.schema.json")
        except ValueError as error:
            raise WorkspaceError(str(error)) from error
        return value


@dataclass(frozen=True)
class WorkspaceConfig:
    """Bounded resource and score policy configuration for a workspace."""

    capacity: int = 4
    ttl_seconds: float = 120.0
    max_candidates: int = 256
    selection_policy: str = POLICY_ID
    weights: Mapping[str, float] = field(default_factory=lambda: dict(DEFAULT_WEIGHTS))
    enabled_factors: frozenset[str] = field(default_factory=lambda: frozenset(SALIENT_FACTORS))

    def __post_init__(self) -> None:
        if self.capacity <= 0:
            raise WorkspaceError("capacity must be positive")
        if self.max_candidates < self.capacity:
            raise WorkspaceError("max_candidates must be at least capacity")
        if not math.isfinite(self.ttl_seconds) or self.ttl_seconds <= 0:
            raise WorkspaceError("ttl_seconds must be finite and positive")
        if self.selection_policy not in SELECTION_POLICIES:
            raise WorkspaceError("selection_policy is not supported")
        if set(self.weights) != set(SALIENT_FACTORS):
            raise WorkspaceError("weights must define every supported salience factor")
        normalized_weights: dict[str, float] = {}
        for name in SALIENT_FACTORS:
            value = self.weights[name]
            if isinstance(value, bool) or not math.isfinite(float(value)) or float(value) <= 0:
                raise WorkspaceError("weights must be finite and positive")
            normalized_weights[name] = float(value)
        normalized_enabled = frozenset(self.enabled_factors)
        if not normalized_enabled <= set(SALIENT_FACTORS):
            raise WorkspaceError("enabled_factors contains an unsupported salience factor")
        object.__setattr__(self, "weights", normalized_weights)
        object.__setattr__(self, "enabled_factors", normalized_enabled)

    @property
    def fingerprint(self) -> str:
        encoded = json.dumps(
            {
                "capacity": self.capacity,
                "ttl_seconds": self.ttl_seconds,
                "max_candidates": self.max_candidates,
                "selection_policy": self.selection_policy,
                "weights": self.weights,
                "enabled_factors": sorted(self.enabled_factors),
            },
            sort_keys=True,
            separators=(",", ":"),
        )
        return hashlib.sha256(encoded.encode("utf-8")).hexdigest()[:16]

    def without_factors(self, *factors: str) -> WorkspaceConfig:
        unknown = set(factors) - set(SALIENT_FACTORS)
        if unknown:
            raise WorkspaceError(f"unsupported ablation factors: {sorted(unknown)}")
        return replace(self, enabled_factors=self.enabled_factors - set(factors))


@dataclass(frozen=True)
class SalienceAssessment:
    policy_id: str
    score: float
    observed_factors: dict[str, float]
    missing_factors: tuple[str, ...]

    def to_mapping(self) -> dict[str, Any]:
        return {
            "policy_id": self.policy_id,
            "score": self.score,
            "observed_factors": dict(self.observed_factors),
            "missing_factors": list(self.missing_factors),
        }


@dataclass(frozen=True)
class SelectionDecision:
    candidate_id: str
    score: float | None
    selected: bool
    rank: int | None
    reason_codes: tuple[str, ...]

    def to_mapping(self) -> dict[str, Any]:
        return {
            "candidate_id": self.candidate_id,
            "score": self.score,
            "selected": self.selected,
            "rank": self.rank,
            "reason_codes": list(self.reason_codes),
        }


@dataclass(frozen=True)
class WorkspaceItem:
    workspace_item_id: str
    schema_version: str
    workspace_id: str
    candidate_id: str
    session_id: str
    source_kind: str
    source_refs: tuple[str, ...]
    observed_at: str
    admitted_at: str
    expires_at: str
    content: dict[str, Any]
    salience: SalienceAssessment
    snapshot_id: str
    rank: int
    selected_at: str
    selection_reasons: tuple[str, ...]

    def to_mapping(self) -> dict[str, Any]:
        value = {
            "workspace_item_id": self.workspace_item_id,
            "schema_version": self.schema_version,
            "workspace_id": self.workspace_id,
            "candidate_id": self.candidate_id,
            "session_id": self.session_id,
            "source_kind": self.source_kind,
            "source_refs": list(self.source_refs),
            "observed_at": self.observed_at,
            "admitted_at": self.admitted_at,
            "expires_at": self.expires_at,
            "content": _copy_json_object(self.content, "content"),
            "salience": self.salience.to_mapping(),
            "selection": {
                "snapshot_id": self.snapshot_id,
                "rank": self.rank,
                "selected_at": self.selected_at,
                "reasons": list(self.selection_reasons),
            },
        }
        try:
            validate_shared_schema(value, "workspace_item.schema.json")
        except ValueError as error:
            raise WorkspaceError(str(error)) from error
        return value


@dataclass(frozen=True)
class WorkspaceSnapshot:
    snapshot_id: str
    schema_version: str
    workspace_id: str
    session_id: str
    created_at: str
    capacity: int
    policy_id: str
    config_fingerprint: str
    selection_churn: float
    active_items: tuple[WorkspaceItem, ...]
    decisions: tuple[SelectionDecision, ...]
    expired_candidate_ids: tuple[str, ...]
    discarded_candidate_ids: tuple[str, ...]

    def to_mapping(self) -> dict[str, Any]:
        value = {
            "snapshot_id": self.snapshot_id,
            "schema_version": self.schema_version,
            "workspace_id": self.workspace_id,
            "session_id": self.session_id,
            "created_at": self.created_at,
            "capacity": self.capacity,
            "policy_id": self.policy_id,
            "config_fingerprint": self.config_fingerprint,
            "selection_churn": self.selection_churn,
            "active_items": [item.to_mapping() for item in self.active_items],
            "decisions": [decision.to_mapping() for decision in self.decisions],
            "expired_candidate_ids": list(self.expired_candidate_ids),
            "discarded_candidate_ids": list(self.discarded_candidate_ids),
        }
        try:
            validate_shared_schema(value, "workspace_snapshot.schema.json")
        except ValueError as error:
            raise WorkspaceError(str(error)) from error
        return value


@dataclass(frozen=True)
class WorkspaceBroadcast:
    broadcast_id: str
    schema_version: str
    workspace_id: str
    session_id: str
    emitted_at: str
    snapshot: WorkspaceSnapshot

    def to_mapping(self) -> dict[str, Any]:
        value = {
            "broadcast_id": self.broadcast_id,
            "schema_version": self.schema_version,
            "workspace_id": self.workspace_id,
            "session_id": self.session_id,
            "emitted_at": self.emitted_at,
            "snapshot": self.snapshot.to_mapping(),
        }
        try:
            validate_shared_schema(value, "workspace_broadcast.schema.json")
        except ValueError as error:
            raise WorkspaceError(str(error)) from error
        return value


@dataclass(frozen=True)
class _Entry:
    candidate: WorkspaceCandidate
    workspace_item_id: str
    admitted_at: datetime
    expires_at: datetime


class GlobalWorkspace:
    """Local short-lived workspace that ranks generic, provenance-backed candidates."""

    def __init__(
        self,
        workspace_id: str,
        session_id: str,
        config: WorkspaceConfig | None = None,
    ) -> None:
        _required_name(workspace_id, "workspace_id")
        _required_name(session_id, "session_id")
        self.workspace_id = workspace_id
        self.session_id = session_id
        self.config = config or WorkspaceConfig()
        self._entries: dict[str, _Entry] = {}
        self._last_active_candidate_ids: frozenset[str] = frozenset()

    def admit(self, candidate: WorkspaceCandidate, now: str | None = None) -> WorkspaceSnapshot:
        """Admit a new immutable candidate, then recompute bounded active contents."""

        moment = _clock(now)
        expired = self._expire(moment)
        if candidate.session_id != self.session_id:
            raise WorkspaceError("candidate session_id does not match the workspace")
        existing = self._entries.get(candidate.candidate_id)
        if existing is not None:
            if existing.candidate.to_mapping() != candidate.to_mapping():
                raise WorkspaceError(
                    "candidate_id is immutable; use update_priority for a priority change"
                )
            return self._build_snapshot(moment, expired, ())
        entry = _Entry(
            candidate=candidate,
            workspace_item_id=str(
                uuid.uuid5(
                    _NAMESPACE,
                    f"{self.workspace_id}:{self.session_id}:{candidate.candidate_id}",
                )
            ),
            admitted_at=moment,
            expires_at=moment + timedelta(seconds=self.config.ttl_seconds),
        )
        self._entries[candidate.candidate_id] = entry
        discarded = self._enforce_resource_bound()
        return self._build_snapshot(moment, expired, discarded)

    def update_priority(
        self,
        candidate_id: str,
        priority: float,
        now: str | None = None,
    ) -> WorkspaceSnapshot:
        """Apply an explicit priority observation without inventing other signals."""

        moment = _clock(now)
        expired = self._expire(moment)
        entry = self._entries.get(candidate_id)
        if entry is None:
            raise WorkspaceError("candidate is unavailable for priority update")
        if isinstance(priority, bool) or not math.isfinite(float(priority)) or not 0 <= float(priority) <= 1:
            raise WorkspaceError("priority must be a finite number between zero and one")
        signals = dict(entry.candidate.salience_signals)
        signals["priority"] = float(priority)
        updated_candidate = WorkspaceCandidate(
            candidate_id=entry.candidate.candidate_id,
            session_id=entry.candidate.session_id,
            source_kind=entry.candidate.source_kind,
            source_refs=entry.candidate.source_refs,
            observed_at=entry.candidate.observed_at,
            content=entry.candidate.content,
            salience_signals=signals,
        )
        self._entries[candidate_id] = replace(entry, candidate=updated_candidate)
        return self._build_snapshot(moment, expired, ())

    def snapshot(self, now: str | None = None) -> WorkspaceSnapshot:
        """Expose the current local state after applying explicit expiration."""

        moment = _clock(now)
        expired = self._expire(moment)
        return self._build_snapshot(moment, expired, ())

    async def broadcast(
        self,
        snapshot: WorkspaceSnapshot,
        publisher: BroadcastPublisher,
        emitted_at: str | None = None,
    ) -> dict[str, Any]:
        """Publish a validated local selection event through an injected bus port."""

        moment = _clock(emitted_at)
        if snapshot.workspace_id != self.workspace_id or snapshot.session_id != self.session_id:
            raise WorkspaceError("snapshot does not belong to this workspace")
        broadcast = WorkspaceBroadcast(
            broadcast_id=str(uuid.uuid5(_NAMESPACE, f"{snapshot.snapshot_id}:{_format_time(moment)}")),
            schema_version=SCHEMA_VERSION,
            workspace_id=self.workspace_id,
            session_id=self.session_id,
            emitted_at=_format_time(moment),
            snapshot=snapshot,
        ).to_mapping()
        event = {
            "schema_version": "1.0",
            "event_id": broadcast["broadcast_id"],
            "source": "global_workspace",
            "event_type": "workspace.selection.v1",
            "occurred_at": broadcast["emitted_at"],
            "monotonic_ns": max(0, int(moment.timestamp() * 1_000_000_000)),
            "received_at": broadcast["emitted_at"],
            "session_id": self.session_id,
            "actor_id": None,
            "context": {"workspace_id": self.workspace_id},
            "payload": broadcast,
            "quality": {"completeness": 1.0, "latency_ms": 0},
            "provenance": {"module": CREATED_BY, "snapshot_id": snapshot.snapshot_id},
            "privacy_class": "local",
            "tags": ["workspace", "selection"],
        }
        await publisher(event)
        return event

    def _expire(self, moment: datetime) -> tuple[str, ...]:
        expired = sorted(
            candidate_id
            for candidate_id, entry in self._entries.items()
            if entry.expires_at <= moment
        )
        for candidate_id in expired:
            del self._entries[candidate_id]
        return tuple(expired)

    def _enforce_resource_bound(self) -> tuple[str, ...]:
        if len(self._entries) <= self.config.max_candidates:
            return ()
        retained = self._rank_entries()[: self.config.max_candidates]
        retained_ids = {entry.candidate.candidate_id for entry, _ in retained}
        discarded = sorted(candidate_id for candidate_id in self._entries if candidate_id not in retained_ids)
        for candidate_id in discarded:
            del self._entries[candidate_id]
        return tuple(discarded)

    def _rank_entries(self) -> list[tuple[_Entry, SalienceAssessment | None]]:
        if self.config.selection_policy == BASELINE_ID:
            return [
                (
                    entry,
                    SalienceAssessment(
                        policy_id=BASELINE_ID,
                        score=0.0,
                        observed_factors={},
                        missing_factors=tuple(sorted(self.config.enabled_factors)),
                    ),
                )
                for entry in sorted(
                    self._entries.values(),
                    key=lambda entry: (entry.admitted_at, entry.candidate.candidate_id),
                )
            ]
        assessments = [(entry, self._assess(entry)) for entry in self._entries.values()]
        return sorted(
            assessments,
            key=lambda value: (
                value[1] is None,
                -(value[1].score if value[1] is not None else 0.0),
                value[0].candidate.candidate_id,
            ),
        )

    def _assess(self, entry: _Entry) -> SalienceAssessment | None:
        observed = {
            name: entry.candidate.salience_signals[name]
            for name in sorted(self.config.enabled_factors)
            if name in entry.candidate.salience_signals
        }
        if not observed:
            return None
        denominator = sum(self.config.weights[name] for name in observed)
        score = sum(observed[name] * self.config.weights[name] for name in observed) / denominator
        return SalienceAssessment(
            policy_id=POLICY_ID,
            score=score,
            observed_factors=observed,
            missing_factors=tuple(sorted(self.config.enabled_factors - set(observed))),
        )

    def _build_snapshot(
        self,
        moment: datetime,
        expired: tuple[str, ...],
        discarded: tuple[str, ...],
    ) -> WorkspaceSnapshot:
        ranked = self._rank_entries()
        selected = [(entry, assessment) for entry, assessment in ranked if assessment is not None][
            : self.config.capacity
        ]
        selected_ids = {entry.candidate.candidate_id for entry, _ in selected}
        selection_churn = _selection_churn(self._last_active_candidate_ids, selected_ids)
        decisions: list[SelectionDecision] = []
        for index, (entry, assessment) in enumerate(ranked, start=1):
            candidate_id = entry.candidate.candidate_id
            if assessment is None:
                decisions.append(
                    SelectionDecision(
                        candidate_id,
                        None,
                        False,
                        None,
                        ("unscored:no_observed_enabled_factor",),
                    )
                )
                continue
            factors = tuple(f"salience.observed:{name}" for name in assessment.observed_factors)
            policy_reason = f"salience.policy:{assessment.policy_id}"
            selection_reason = (
                "selection.fifo_admission"
                if assessment.policy_id == BASELINE_ID
                else "selection.capacity"
            )
            if candidate_id in selected_ids:
                decisions.append(
                    SelectionDecision(
                        candidate_id,
                        assessment.score,
                        True,
                        index,
                        (policy_reason, *factors, selection_reason),
                    )
                )
            else:
                decisions.append(
                    SelectionDecision(
                        candidate_id,
                        assessment.score,
                        False,
                        index,
                        (policy_reason, *factors, "capacity.excluded"),
                    )
                )
        snapshot_id = self._snapshot_id(
            moment,
            decisions,
            expired,
            discarded,
            selection_churn,
        )
        selected_at = _format_time(moment)
        active_items = tuple(
            WorkspaceItem(
                workspace_item_id=entry.workspace_item_id,
                schema_version=SCHEMA_VERSION,
                workspace_id=self.workspace_id,
                candidate_id=entry.candidate.candidate_id,
                session_id=entry.candidate.session_id,
                source_kind=entry.candidate.source_kind,
                source_refs=entry.candidate.source_refs,
                observed_at=entry.candidate.observed_at,
                admitted_at=_format_time(entry.admitted_at),
                expires_at=_format_time(entry.expires_at),
                content=_copy_json_object(entry.candidate.content, "content"),
                salience=assessment,
                snapshot_id=snapshot_id,
                rank=rank,
                selected_at=selected_at,
                selection_reasons=next(
                    decision.reason_codes
                    for decision in decisions
                    if decision.candidate_id == entry.candidate.candidate_id
                ),
            )
            for rank, (entry, assessment) in enumerate(selected, start=1)
        )
        snapshot = WorkspaceSnapshot(
            snapshot_id=snapshot_id,
            schema_version=SCHEMA_VERSION,
            workspace_id=self.workspace_id,
            session_id=self.session_id,
            created_at=selected_at,
            capacity=self.config.capacity,
            policy_id=self.config.selection_policy,
            config_fingerprint=self.config.fingerprint,
            selection_churn=selection_churn,
            active_items=active_items,
            decisions=tuple(decisions),
            expired_candidate_ids=expired,
            discarded_candidate_ids=discarded,
        )
        self._last_active_candidate_ids = frozenset(selected_ids)
        return snapshot

    def _snapshot_id(
        self,
        moment: datetime,
        decisions: Sequence[SelectionDecision],
        expired: Sequence[str],
        discarded: Sequence[str],
        selection_churn: float,
    ) -> str:
        payload = {
            "workspace_id": self.workspace_id,
            "session_id": self.session_id,
            "created_at": _format_time(moment),
            "config_fingerprint": self.config.fingerprint,
            "decisions": [decision.to_mapping() for decision in decisions],
            "expired": list(expired),
            "discarded": list(discarded),
            "selection_churn": selection_churn,
        }
        encoded = json.dumps(payload, sort_keys=True, separators=(",", ":"))
        return str(uuid.uuid5(_NAMESPACE, encoded))


def fifo_capacity_baseline(
    candidates: Sequence[WorkspaceCandidate], capacity: int
) -> tuple[str, ...]:
    """Control policy for experiments: retain admission order without salience."""

    if capacity <= 0:
        raise WorkspaceError("capacity must be positive")
    return tuple(candidate.candidate_id for candidate in candidates[:capacity])


def evaluate_selection(
    snapshot: WorkspaceSnapshot,
    annotated_relevant_ids: Collection[str],
) -> dict[str, Any]:
    """Score a selection against frozen annotations without claiming validity alone."""

    relevant = set(annotated_relevant_ids)
    if any(not isinstance(value, str) or not value.strip() for value in relevant):
        raise WorkspaceError("annotated relevant ids must be non-empty strings")
    selected = {item.candidate_id for item in snapshot.active_items}
    true_positive = len(selected & relevant)
    precision = true_positive / len(selected) if selected else (1.0 if not relevant else 0.0)
    recall = true_positive / len(relevant) if relevant else (1.0 if not selected else 0.0)
    f1 = 0.0 if precision + recall == 0 else 2 * precision * recall / (precision + recall)
    return {
        "baseline_id": BASELINE_ID,
        "policy_id": snapshot.policy_id,
        "registered": True,
        "hypothesis": HYPOTHESIS,
        "ablation": ABLATION,
        "falsification": FALSIFICATION,
        "precision_at_capacity": precision,
        "recall_at_capacity": recall,
        "selection_f1": f1,
        "active_item_count": len(selected),
        "capacity": snapshot.capacity,
        "occupancy": len(selected) / snapshot.capacity,
        "selection_churn": snapshot.selection_churn,
    }


def _normalize_signals(signals: Mapping[str, float]) -> dict[str, float]:
    if not isinstance(signals, Mapping) or not signals:
        raise WorkspaceError("at least one observed salience signal is required")
    unknown = set(signals) - set(SALIENT_FACTORS)
    if unknown:
        raise WorkspaceError(f"unsupported salience signals: {sorted(unknown)}")
    normalized: dict[str, float] = {}
    for name in sorted(signals):
        value = signals[name]
        if isinstance(value, bool) or not math.isfinite(float(value)) or not 0 <= float(value) <= 1:
            raise WorkspaceError("salience signals must be finite numbers between zero and one")
        normalized[name] = float(value)
    return normalized


def _copy_json_object(value: Mapping[str, Any], name: str) -> dict[str, Any]:
    if not isinstance(value, Mapping):
        raise WorkspaceError(f"{name} must be an object")
    try:
        copied = json.loads(json.dumps(dict(value), ensure_ascii=False, sort_keys=True))
    except (TypeError, ValueError) as error:
        raise WorkspaceError(f"{name} must contain JSON-compatible values") from error
    if not isinstance(copied, dict):
        raise WorkspaceError(f"{name} must be an object")
    return copied


def _required_name(value: str, name: str) -> None:
    if not isinstance(value, str) or not value.strip():
        raise WorkspaceError(f"{name} must be a non-empty string")


def _clock(value: str | None) -> datetime:
    return datetime.now(UTC) if value is None else _parse_time(value, "timestamp")


def _parse_time(value: str, name: str) -> datetime:
    if not isinstance(value, str):
        raise WorkspaceError(f"{name} must be an ISO-8601 string")
    try:
        parsed = datetime.fromisoformat(value)
    except ValueError as error:
        raise WorkspaceError(f"{name} must be a valid ISO-8601 timestamp") from error
    if parsed.tzinfo is None:
        raise WorkspaceError(f"{name} must include a timezone")
    return parsed.astimezone(UTC)


def _format_time(value: datetime) -> str:
    return value.astimezone(UTC).isoformat()


def _selection_churn(previous: Collection[str], current: Collection[str]) -> float:
    combined = set(previous) | set(current)
    if not combined:
        return 0.0
    return len(set(previous) ^ set(current)) / len(combined)
