"""Local, reversible replay and consolidation reference for the research lab."""

from __future__ import annotations

import hashlib
import time
from collections.abc import Mapping, Sequence
from dataclasses import dataclass
from datetime import datetime
from enum import Enum
from typing import Any

from .schema_validation import validate_shared_schema

SCHEMA_VERSION = "1.0"
CONSOLIDATION_POLICY_ID = "replay_with_provenance_v1"
BASELINE_POLICY_ID = "no_replay_v0"
HYPOTHESIS = "replay and consolidation reduce forgetting and contradictions"
ABLATION = "replace replay_with_provenance_v1 with no_replay_v0"
FALSIFICATION = (
    "treatment does not improve retention, or consolidation increases "
    "unsupported knowledge and contradictions"
)


class MemoryConsolidationError(ValueError):
    """Raised for invalid consolidation, replay, or retention transitions."""


class ConsolidationPolicy(str, Enum):
    replay_with_provenance_v1 = CONSOLIDATION_POLICY_ID
    no_replay_v0 = BASELINE_POLICY_ID


@dataclass(frozen=True)
class SemanticKnowledge:
    knowledge_id: str
    concept: str
    source_episode_ids: tuple[str, ...]
    support_count: int
    confidence: float
    version: int
    alternatives: tuple[str, ...]
    contradictions: tuple[str, ...]
    created_at: str
    schema_version: str = SCHEMA_VERSION

    def __post_init__(self) -> None:
        _required(self.knowledge_id, "knowledge_id")
        _required(self.concept, "concept")
        _strings(self.source_episode_ids, "source_episode_ids")
        if self.support_count < 1:
            raise MemoryConsolidationError("support_count must be positive")
        _probability(self.confidence, "confidence")
        if self.version < 1:
            raise MemoryConsolidationError("version must be positive")
        _strings(self.alternatives, "alternatives", allow_empty=True)
        _strings(self.contradictions, "contradictions", allow_empty=True)
        _time(self.created_at, "created_at")
        if self.schema_version != SCHEMA_VERSION:
            raise MemoryConsolidationError("unsupported knowledge schema version")

    def to_mapping(self) -> dict[str, Any]:
        value = {
            "schema_version": self.schema_version,
            "knowledge_id": self.knowledge_id,
            "concept": self.concept,
            "source_episode_ids": list(self.source_episode_ids),
            "support_count": self.support_count,
            "confidence": self.confidence,
            "version": self.version,
            "alternatives": list(self.alternatives),
            "contradictions": list(self.contradictions),
            "created_at": self.created_at,
        }
        validate_shared_schema(value, "semantic_knowledge.schema.json")
        return value


@dataclass(frozen=True)
class ConsolidationRecord:
    record_id: str
    policy_id: str
    source_episode_ids: tuple[str, ...]
    knowledge_ids: tuple[str, ...]
    replayed_at: str
    processing_cost_ms: float
    schema_version: str = SCHEMA_VERSION

    def __post_init__(self) -> None:
        _required(self.record_id, "record_id")
        _required(self.policy_id, "policy_id")
        _strings(self.source_episode_ids, "source_episode_ids")
        _strings(self.knowledge_ids, "knowledge_ids", allow_empty=True)
        _time(self.replayed_at, "replayed_at")
        if self.processing_cost_ms < 0:
            raise MemoryConsolidationError("processing_cost_ms must be non-negative")
        if self.schema_version != SCHEMA_VERSION:
            raise MemoryConsolidationError("unsupported consolidation schema version")

    def to_mapping(self) -> dict[str, Any]:
        value = {
            "schema_version": self.schema_version,
            "record_id": self.record_id,
            "policy_id": self.policy_id,
            "source_episode_ids": list(self.source_episode_ids),
            "knowledge_ids": list(self.knowledge_ids),
            "replayed_at": self.replayed_at,
            "processing_cost_ms": self.processing_cost_ms,
        }
        validate_shared_schema(value, "consolidation_record.schema.json")
        return value


@dataclass(frozen=True)
class RetentionDecision:
    decision_id: str
    episode_id: str
    action: str
    reason: str
    reversible: bool
    prior_state: str
    occurred_at: str
    schema_version: str = SCHEMA_VERSION

    def __post_init__(self) -> None:
        for name, value in (
            ("decision_id", self.decision_id),
            ("episode_id", self.episode_id),
            ("reason", self.reason),
        ):
            _required(value, name)
        if self.action not in {"retain", "archive", "restore"}:
            raise MemoryConsolidationError("unsupported retention action")
        if self.prior_state not in {"active", "archived"}:
            raise MemoryConsolidationError("unsupported retention prior_state")
        _time(self.occurred_at, "occurred_at")
        if self.schema_version != SCHEMA_VERSION:
            raise MemoryConsolidationError("unsupported retention schema version")

    def to_mapping(self) -> dict[str, Any]:
        value = {
            "schema_version": self.schema_version,
            "decision_id": self.decision_id,
            "episode_id": self.episode_id,
            "action": self.action,
            "reason": self.reason,
            "reversible": self.reversible,
            "prior_state": self.prior_state,
            "occurred_at": self.occurred_at,
        }
        validate_shared_schema(value, "retention_decision.schema.json")
        return value


class MemoryConsolidator:
    """Replay observed episode context without deleting its source records."""

    def __init__(
        self,
        policy: ConsolidationPolicy = ConsolidationPolicy.replay_with_provenance_v1,
        *,
        max_active_episodes: int = 10_000,
    ) -> None:
        try:
            self.policy = ConsolidationPolicy(policy)
        except ValueError as error:
            raise MemoryConsolidationError("unsupported consolidation policy") from error
        if max_active_episodes <= 0:
            raise MemoryConsolidationError("max_active_episodes must be positive")
        self.max_active_episodes = max_active_episodes
        self._knowledge: dict[str, SemanticKnowledge] = {}
        self._episodes: dict[str, dict[str, Any]] = {}
        self._archived: set[str] = set()
        self._records: list[ConsolidationRecord] = []
        self._retention: list[RetentionDecision] = []

    def replay(
        self, episodes: Sequence[Mapping[str, Any]], replayed_at: str
    ) -> ConsolidationRecord:
        _time(replayed_at, "replayed_at")
        started = time.perf_counter()
        normalized = [_episode(value) for value in episodes]
        normalized.sort(key=lambda value: (value["end_at"], value["episode_id"]))
        for value in normalized:
            self._episodes[value["episode_id"]] = value
        knowledge_ids: list[str] = []
        if self.policy is ConsolidationPolicy.replay_with_provenance_v1:
            groups: dict[str, list[dict[str, Any]]] = {}
            for episode in normalized:
                for concept in _observed_concepts(episode):
                    groups.setdefault(concept, []).append(episode)
            for concept in sorted(groups):
                knowledge = self._reconcile(concept, groups[concept], replayed_at)
                self._knowledge[knowledge.knowledge_id] = knowledge
                knowledge_ids.append(knowledge.knowledge_id)
        record = ConsolidationRecord(
            record_id=f"consolidation-{len(self._records) + 1}",
            policy_id=self.policy.value,
            source_episode_ids=tuple(value["episode_id"] for value in normalized),
            knowledge_ids=tuple(sorted(knowledge_ids)),
            replayed_at=replayed_at,
            processing_cost_ms=(time.perf_counter() - started) * 1000,
        )
        record.to_mapping()
        self._records.append(record)
        return record

    def apply_retention(self, occurred_at: str) -> tuple[RetentionDecision, ...]:
        _time(occurred_at, "occurred_at")
        ordered = sorted(
            self._episodes.values(),
            key=lambda value: (value["end_at"], value["episode_id"]),
            reverse=True,
        )
        keep = {value["episode_id"] for value in ordered[: self.max_active_episodes]}
        decisions: list[RetentionDecision] = []
        for episode_id in sorted(self._episodes):
            prior = "archived" if episode_id in self._archived else "active"
            if episode_id in keep:
                action = "retain"
                self._archived.discard(episode_id)
                reason = "within_active_episode_budget"
            else:
                action = "archive"
                self._archived.add(episode_id)
                reason = "outside_active_episode_budget"
            decision = RetentionDecision(
                decision_id=f"retention-{len(self._retention) + len(decisions) + 1}",
                episode_id=episode_id,
                action=action,
                reason=reason,
                reversible=True,
                prior_state=prior,
                occurred_at=occurred_at,
            )
            decision.to_mapping()
            decisions.append(decision)
        self._retention.extend(decisions)
        return tuple(decisions)

    def restore(self, episode_id: str, occurred_at: str) -> RetentionDecision:
        _required(episode_id, "episode_id")
        _time(occurred_at, "occurred_at")
        if episode_id not in self._episodes:
            raise MemoryConsolidationError("unknown episode cannot be restored")
        prior = "archived" if episode_id in self._archived else "active"
        self._archived.discard(episode_id)
        decision = RetentionDecision(
            decision_id=f"retention-{len(self._retention) + 1}",
            episode_id=episode_id,
            action="restore",
            reason="explicit_reversible_restore",
            reversible=True,
            prior_state=prior,
            occurred_at=occurred_at,
        )
        decision.to_mapping()
        self._retention.append(decision)
        return decision

    def knowledge(self, concept: str | None = None) -> tuple[SemanticKnowledge, ...]:
        values = tuple(self._knowledge.values())
        if concept is not None:
            values = tuple(value for value in values if value.concept == concept)
        return tuple(sorted(values, key=lambda value: value.knowledge_id))

    @property
    def archived_episode_ids(self) -> tuple[str, ...]:
        return tuple(sorted(self._archived))

    @property
    def records(self) -> tuple[ConsolidationRecord, ...]:
        return tuple(self._records)

    @property
    def retention_decisions(self) -> tuple[RetentionDecision, ...]:
        return tuple(self._retention)

    def metrics(self, expected_concepts: Sequence[str]) -> dict[str, float | str]:
        expected = set(expected_concepts)
        known = {value.concept for value in self._knowledge.values()}
        retention = len(expected & known) / len(expected) if expected else 1.0
        contradictions = sum(bool(value.contradictions) for value in self._knowledge.values())
        rate = contradictions / len(self._knowledge) if self._knowledge else 0.0
        return {
            "policy_id": self.policy.value,
            "retention_score": retention,
            "contradiction_rate": rate,
            "knowledge_count": float(len(self._knowledge)),
        }

    @staticmethod
    def scientific_metadata() -> dict[str, str]:
        return {
            "hypothesis": HYPOTHESIS,
            "baseline_id": BASELINE_POLICY_ID,
            "ablation": ABLATION,
            "falsification": FALSIFICATION,
            "primary_metric": "retention_score",
        }

    def _reconcile(
        self, concept: str, episodes: Sequence[Mapping[str, Any]], created_at: str
    ) -> SemanticKnowledge:
        knowledge_id = "knowledge-" + hashlib.sha256(concept.encode("utf-8")).hexdigest()[:16]
        previous = self._knowledge.get(knowledge_id)
        source_ids = {str(value["episode_id"]) for value in episodes}
        alternatives = {
            f"hypothesis:{hypothesis}"
            for value in episodes
            for hypothesis in value.get("hypotheses", [])
        }
        contradictions: set[str] = set()
        if previous is not None:
            source_ids.update(previous.source_episode_ids)
            alternatives.update(previous.alternatives)
            contradictions.update(previous.contradictions)
        if len(alternatives) > 1:
            contradictions.add("multiple_hypotheses")
        version = previous.version + 1 if previous is not None else 1
        return SemanticKnowledge(
            knowledge_id=knowledge_id,
            concept=concept,
            source_episode_ids=tuple(sorted(source_ids)),
            support_count=len(source_ids),
            confidence=min(1.0, 0.5 + 0.1 * (len(source_ids) - 1)),
            version=version,
            alternatives=tuple(sorted(alternatives)),
            contradictions=tuple(sorted(contradictions)),
            created_at=created_at,
        )


def _observed_concepts(episode: Mapping[str, Any]) -> tuple[str, ...]:
    context = episode["context_summary"]
    concepts = {
        f"{field}:{value}"
        for field in ("applications", "documents", "topics")
        for value in context.get(field, [])
        if isinstance(value, str) and value
    }
    return tuple(sorted(concepts))


def _episode(value: Mapping[str, Any]) -> dict[str, Any]:
    if not isinstance(value, Mapping):
        raise MemoryConsolidationError("episode must be a mapping")
    normalized = dict(value)
    try:
        validate_shared_schema(normalized, "episode.schema.json")
    except ValueError as error:
        raise MemoryConsolidationError(str(error)) from error
    return normalized


def _required(value: str | None, name: str) -> None:
    if not isinstance(value, str) or not value.strip():
        raise MemoryConsolidationError(f"{name} is required")


def _strings(values: Sequence[str], name: str, *, allow_empty: bool = False) -> None:
    if (not allow_empty and not values) or any(
        not isinstance(value, str) or not value.strip() for value in values
    ):
        raise MemoryConsolidationError(f"{name} must contain non-empty strings")


def _probability(value: float, name: str) -> None:
    if isinstance(value, bool) or not isinstance(value, (int, float)) or not 0 <= value <= 1:
        raise MemoryConsolidationError(f"{name} must be between zero and one")


def _time(value: str, name: str) -> None:
    try:
        parsed = datetime.fromisoformat(value)
    except ValueError as error:
        raise MemoryConsolidationError(f"{name} must be ISO-8601") from error
    if parsed.tzinfo is None or parsed.tzinfo.utcoffset(parsed) is None:
        raise MemoryConsolidationError(f"{name} must include timezone")
