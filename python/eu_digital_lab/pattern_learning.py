"""Incremental, task-agnostic pattern learning baseline.

The learner clusters numeric observations by distance and promotes a cluster
only after configurable support. It records versions and human feedback, but
does not name patterns, execute actions, or treat a cluster as a fact about the
world.
"""

from __future__ import annotations

import math
import uuid
from collections.abc import Mapping
from dataclasses import dataclass
from datetime import datetime
from enum import Enum
from typing import Any

from .schema_validation import validate_shared_schema

SCHEMA_VERSION = "1.0"
BASELINE_ID = "online_exact_threshold_v1"
CREATED_BY = "pattern_learner.distance_threshold.v1"
FALSIFICATION = "incremental clustering does not beat the exact-key baseline"
ABLATION = "set distance_threshold to zero and disable cross-feature clustering"
_NAMESPACE = uuid.UUID("e756dcc0-b35a-43f6-a7d1-30e89f4f1b55")


class PatternLearningError(ValueError):
    """Raised for invalid observations, feedback, or learner state."""


class PatternStatus(str, Enum):
    candidate = "candidate"
    promoted = "promoted"
    superseded = "superseded"


@dataclass(frozen=True)
class PatternConfig:
    min_support: int = 3
    distance_threshold: float = 0.25
    promotion_confidence: float = 0.5

    def __post_init__(self) -> None:
        if self.min_support < 1:
            raise ValueError("min_support must be positive")
        if not math.isfinite(self.distance_threshold) or self.distance_threshold < 0:
            raise ValueError("distance_threshold must be finite and non-negative")
        if not 0 <= self.promotion_confidence <= 1:
            raise ValueError("promotion_confidence must be between zero and one")


@dataclass
class PatternRecord:
    pattern_id: str
    schema_version: str
    version: int
    status: PatternStatus
    centroid: dict[str, float]
    support: int
    stability: float
    recency: float
    confidence: float
    observation_refs: list[str]
    feedback: dict[str, Any]
    parent_pattern_id: str | None
    drift_reason: str | None
    created_by: str
    _last_seen_index: int = 0

    def to_mapping(self) -> dict[str, Any]:
        value = {
            "pattern_id": self.pattern_id,
            "schema_version": self.schema_version,
            "version": self.version,
            "status": self.status.value,
            "centroid": dict(sorted(self.centroid.items())),
            "support": self.support,
            "stability": self.stability,
            "recency": self.recency,
            "confidence": self.confidence,
            "observation_refs": list(self.observation_refs),
            "feedback": {
                "positive": int(self.feedback["positive"]),
                "negative": int(self.feedback["negative"]),
                "references": list(self.feedback["references"]),
            },
            "parent_pattern_id": self.parent_pattern_id,
            "drift_reason": self.drift_reason,
            "created_by": self.created_by,
        }
        validate_shared_schema(value, "pattern.schema.json")
        return value


class PatternLearner:
    """Online distance baseline with explicit promotion and drift versions."""

    def __init__(self, config: PatternConfig | None = None, *, stream_id: str) -> None:
        if not stream_id.strip():
            raise ValueError("stream_id cannot be empty")
        self.config = config or PatternConfig()
        self.stream_id = stream_id
        self._patterns: dict[str, PatternRecord] = {}
        self._clock = 0

    def observe(self, features: Mapping[str, float], observation_ref: str, occurred_at: str) -> PatternRecord:
        vector = _normalize_features(features)
        if not observation_ref.strip():
            raise PatternLearningError("observation_ref cannot be empty")
        _parse_time(occurred_at)
        self._clock += 1
        for record in self._patterns.values():
            if record.status != PatternStatus.superseded:
                record.recency = max(0.0, record.recency * 0.99)

        nearest = self._nearest(vector)
        if nearest is not None:
            self._update(nearest, vector, observation_ref)
            return _copy_record(nearest)

        parent = self._closest_historical(vector)
        if parent is not None:
            parent.status = PatternStatus.superseded
            version = parent.version + 1
            drift_reason = "concept_drift"
            parent_id = parent.pattern_id
        else:
            version = 1
            drift_reason = None
            parent_id = None
        pattern_id = str(uuid.uuid5(_NAMESPACE, f"{self.stream_id}:{parent_id or 'root'}:{version}"))
        record = PatternRecord(
            pattern_id=pattern_id,
            schema_version=SCHEMA_VERSION,
            version=version,
            status=PatternStatus.candidate,
            centroid=dict(vector),
            support=1,
            stability=min(1.0, 1.0 / self.config.min_support),
            recency=1.0,
            confidence=0.5,
            observation_refs=[observation_ref],
            feedback={"positive": 0, "negative": 0, "references": []},
            parent_pattern_id=parent_id,
            drift_reason=drift_reason,
            created_by=CREATED_BY,
            _last_seen_index=self._clock,
        )
        self._patterns[pattern_id] = record
        self._refresh_status(record)
        return _copy_record(record)

    def feedback(self, pattern_id: str, *, positive: bool, reference: str) -> PatternRecord:
        if not reference.strip():
            raise PatternLearningError("feedback reference cannot be empty")
        record = self._patterns.get(pattern_id)
        if record is None:
            raise PatternLearningError(f"unknown pattern: {pattern_id}")
        key = "positive" if positive else "negative"
        record.feedback[key] += 1
        record.feedback["references"].append(reference)
        total = record.feedback["positive"] + record.feedback["negative"]
        record.confidence = (record.feedback["positive"] + 1) / (total + 2)
        self._refresh_status(record)
        return _copy_record(record)

    def metrics(self) -> dict[str, Any]:
        clusters = []
        for record in sorted(self._patterns.values(), key=lambda item: (item.pattern_id, item.version)):
            total_feedback = record.feedback["positive"] + record.feedback["negative"]
            clusters.append(
                {
                    "pattern_id": record.pattern_id,
                    "version": record.version,
                    "status": record.status.value,
                    "support": record.support,
                    "stability": record.stability,
                    "recency": record.recency,
                    "confidence": record.confidence,
                    "false_discovery_rate": record.feedback["negative"] / total_feedback if total_feedback else 0.0,
                }
            )
        return {
            "baseline_id": BASELINE_ID,
            "registered": True,
            "clusters": clusters,
            "ablation": ABLATION,
            "falsification": FALSIFICATION,
        }

    def snapshot(self) -> dict[str, Any]:
        return {
            "schema_version": SCHEMA_VERSION,
            "stream_id": self.stream_id,
            "patterns": [
                record.to_mapping()
                for record in sorted(self._patterns.values(), key=lambda item: (item.pattern_id, item.version))
            ],
        }

    def _nearest(self, vector: dict[str, float]) -> PatternRecord | None:
        candidates = [record for record in self._patterns.values() if record.status != PatternStatus.superseded]
        distances = [(distance, record) for record in candidates if (distance := _distance(vector, record.centroid)) is not None]
        if not distances:
            return None
        distance, record = min(distances, key=lambda item: (item[0], item[1].pattern_id))
        return record if distance <= self.config.distance_threshold else None

    def _closest_historical(self, vector: dict[str, float]) -> PatternRecord | None:
        distances = [(distance, record) for record in self._patterns.values() if (distance := _distance(vector, record.centroid)) is not None]
        if not distances:
            return None
        return min(distances, key=lambda item: (item[0], item[1].pattern_id))[1]

    def _update(self, record: PatternRecord, vector: dict[str, float], observation_ref: str) -> None:
        old_support = record.support
        for key, value in vector.items():
            previous = record.centroid.get(key, value)
            record.centroid[key] = (previous * old_support + value) / (old_support + 1)
        record.support += 1
        record.stability = min(1.0, record.support / self.config.min_support)
        record.recency = 1.0
        record._last_seen_index = self._clock
        record.observation_refs.append(observation_ref)
        self._refresh_status(record)

    def _refresh_status(self, record: PatternRecord) -> None:
        if record.status == PatternStatus.superseded:
            return
        record.status = (
            PatternStatus.promoted
            if record.support >= self.config.min_support and record.confidence >= self.config.promotion_confidence
            else PatternStatus.candidate
        )


def _copy_record(record: PatternRecord) -> PatternRecord:
    return PatternRecord(
        pattern_id=record.pattern_id,
        schema_version=record.schema_version,
        version=record.version,
        status=record.status,
        centroid=dict(record.centroid),
        support=record.support,
        stability=record.stability,
        recency=record.recency,
        confidence=record.confidence,
        observation_refs=list(record.observation_refs),
        feedback={key: list(value) if isinstance(value, list) else value for key, value in record.feedback.items()},
        parent_pattern_id=record.parent_pattern_id,
        drift_reason=record.drift_reason,
        created_by=record.created_by,
        _last_seen_index=record._last_seen_index,
    )


def _normalize_features(features: Mapping[str, float]) -> dict[str, float]:
    if not features:
        raise PatternLearningError("at least one numeric feature is required")
    result: dict[str, float] = {}
    for key, value in features.items():
        if not isinstance(key, str) or not key.strip() or isinstance(value, bool):
            raise PatternLearningError("features must map non-empty names to numbers")
        numeric = float(value)
        if not math.isfinite(numeric):
            raise PatternLearningError("features must be finite")
        result[key] = numeric
    return dict(sorted(result.items()))


def _distance(first: Mapping[str, float], second: Mapping[str, float]) -> float | None:
    shared = sorted(set(first) & set(second))
    if not shared:
        return None
    return math.sqrt(sum((first[key] - second[key]) ** 2 for key in shared) / len(shared))


def _parse_time(value: str) -> datetime:
    try:
        parsed = datetime.fromisoformat(value)
    except ValueError as error:
        raise PatternLearningError(f"invalid observation timestamp: {value}") from error
    if parsed.tzinfo is None:
        raise PatternLearningError("observation timestamps must include timezone")
    return parsed
