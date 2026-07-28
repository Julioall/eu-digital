"""Deterministic, explainable episode segmentation for the research lab.

This module is a threshold baseline, not a claim of learned cognition. Its
testable hypothesis is that elapsed-time gaps plus explicitly observed
application/document changes provide a reproducible episode boundary signal.
The baseline is falsified if multimodal fusion does not outperform the best
single-modality baseline on held-out annotated sessions.
"""

from __future__ import annotations

import hashlib
import json
import math
import uuid
from collections.abc import Mapping, Sequence
from dataclasses import asdict, dataclass
from datetime import datetime
from itertools import pairwise
from typing import Any

from .schema_validation import validate_shared_schema

SCHEMA_VERSION = "1.0"
BASELINE_ID = "time_context_threshold_v1"
CREATED_BY = "episode_segmenter.threshold.v1"
HYPOTHESIS = (
    "elapsed-time gaps plus observed application/document changes produce "
    "reproducible episode boundaries"
)
FALSIFICATION = "fusion does not exceed the best single-modality baseline"
ABLATION = "disable application/document context splits and compare against time-only"
_NAMESPACE = uuid.UUID("4f254c43-59a0-48bc-9e17-0f145f9ecac4")


class SegmentationError(ValueError):
    """Raised when an event stream cannot be segmented without guessing."""


@dataclass(frozen=True)
class SegmentConfig:
    max_gap_seconds: float = 300.0
    split_on_application_change: bool = True
    split_on_document_change: bool = True

    def __post_init__(self) -> None:
        if not math.isfinite(self.max_gap_seconds) or self.max_gap_seconds <= 0:
            raise ValueError("max_gap_seconds must be finite and positive")

    @property
    def fingerprint(self) -> str:
        encoded = json.dumps(asdict(self), sort_keys=True, separators=(",", ":"))
        return hashlib.sha256(encoded.encode("utf-8")).hexdigest()[:16]


@dataclass(frozen=True)
class BoundaryDecision:
    event_id: str
    reasons: tuple[str, ...]

    def to_mapping(self) -> dict[str, Any]:
        return {"event_id": self.event_id, "reasons": list(self.reasons)}


@dataclass(frozen=True)
class SegmentedEpisode:
    episode_id: str
    schema_version: str
    session_id: str
    start_at: str
    end_at: str
    event_ids: list[str]
    context_summary: dict[str, list[str]]
    boundary_reasons: list[str]
    embedding_ref: str | None
    summary: str | None
    hypotheses: list[str]
    quality: dict[str, float]
    created_by: str

    def to_mapping(self) -> dict[str, Any]:
        value = asdict(self)
        validate_shared_schema(value, "episode.schema.json")
        return value


@dataclass(frozen=True)
class SegmentationResult:
    schema_version: str
    baseline_id: str
    config_fingerprint: str
    episodes: list[SegmentedEpisode]
    boundaries: list[BoundaryDecision]

    def to_mapping(self) -> dict[str, Any]:
        return {
            "schema_version": self.schema_version,
            "baseline_id": self.baseline_id,
            "config_fingerprint": self.config_fingerprint,
            "episodes": [episode.to_mapping() for episode in self.episodes],
            "boundaries": [boundary.to_mapping() for boundary in self.boundaries],
        }


@dataclass(frozen=True)
class _NormalizedEvent:
    event_id: str
    session_id: str
    occurred_at: str
    timestamp: datetime
    application: str | None
    document: str | None
    modality: str


def segment_events(
    events: Sequence[Mapping[str, Any]], config: SegmentConfig | None = None
) -> SegmentationResult:
    """Segment one ordered session without inferring missing observations."""

    active_config = config or SegmentConfig()
    if not events:
        return SegmentationResult(SCHEMA_VERSION, BASELINE_ID, active_config.fingerprint, [], [])

    normalized = [_normalize_event(event) for event in events]
    session_id = normalized[0].session_id
    if not session_id:
        raise SegmentationError("session_id is required")
    if any(event.session_id != session_id for event in normalized):
        raise SegmentationError("events from multiple sessions cannot be segmented together")
    if len({event.event_id for event in normalized}) != len(normalized):
        raise SegmentationError("event_ids must be unique")
    for previous, current in pairwise(normalized):
        if current.timestamp < previous.timestamp:
            raise SegmentationError("events must be ordered by occurred_at")

    boundaries = [BoundaryDecision(normalized[0].event_id, ("episode_start",))]
    episode_ranges: list[tuple[int, int, list[str]]] = []
    start = 0
    previous = normalized[0]
    previous_application = previous.application
    previous_document = previous.document

    for index, current in enumerate(normalized[1:], start=1):
        reasons: list[str] = []
        if (current.timestamp - previous.timestamp).total_seconds() > active_config.max_gap_seconds:
            reasons.append("time_gap")
        if (
            active_config.split_on_application_change
            and current.application is not None
            and previous_application is not None
            and current.application != previous_application
        ):
            reasons.append("context_change:application")
        if (
            active_config.split_on_document_change
            and current.document is not None
            and previous_document is not None
            and current.document != previous_document
        ):
            reasons.append("context_change:document")

        if reasons:
            boundaries.append(BoundaryDecision(current.event_id, tuple(reasons)))
            episode_ranges.append((start, index, list(boundaries[-2].reasons)))
            start = index
        if current.application is not None:
            previous_application = current.application
        if current.document is not None:
            previous_document = current.document
        previous = current

    episode_ranges.append((start, len(normalized), list(boundaries[-1].reasons)))
    episodes = [
        _make_episode(session_id, normalized[episode_start:episode_end], reasons, active_config)
        for episode_start, episode_end, reasons in episode_ranges
    ]
    return SegmentationResult(SCHEMA_VERSION, BASELINE_ID, active_config.fingerprint, episodes, boundaries)


def evaluate_baseline(
    events: Sequence[Mapping[str, Any]],
    annotated_episodes: Sequence[Mapping[str, Any]],
    config: SegmentConfig | None = None,
) -> dict[str, Any]:
    """Register the baseline and score it against annotated episode boundaries."""

    result = segment_events(events, config)
    event_ids = [_required_string(event, "event_id") for event in events]
    metrics = boundary_metrics(result.episodes, annotated_episodes, event_ids)
    metrics.update(
        {
            "baseline_id": BASELINE_ID,
            "registered": True,
            "hypothesis": HYPOTHESIS,
            "falsification": FALSIFICATION,
            "ablation": ABLATION,
            "config_fingerprint": result.config_fingerprint,
        }
    )
    return metrics


def boundary_metrics(
    predicted_episodes: Sequence[SegmentedEpisode | Mapping[str, Any]],
    reference_episodes: Sequence[Mapping[str, Any]],
    event_ids: Sequence[str],
) -> dict[str, Any]:
    """Calculate boundary precision/recall/F1 and WindowDiff."""

    predicted = _boundary_positions(predicted_episodes, event_ids)
    reference = _boundary_positions(reference_episodes, event_ids)
    true_positive = len(predicted & reference)
    precision = true_positive / len(predicted) if predicted else (1.0 if not reference else 0.0)
    recall = true_positive / len(reference) if reference else (1.0 if not predicted else 0.0)
    f1 = 0.0 if precision + recall == 0 else 2 * precision * recall / (precision + recall)
    return {
        "boundary_precision": precision,
        "boundary_recall": recall,
        "boundary_f1": f1,
        "window_diff": _window_diff(predicted, reference, len(event_ids)),
        "predicted_boundary_count": len(predicted),
        "reference_boundary_count": len(reference),
    }


def _normalize_event(event: Mapping[str, Any]) -> _NormalizedEvent:
    event_id = _required_string(event, "event_id")
    session_id = _required_string(event, "session_id")
    occurred_at = _required_string(event, "occurred_at")
    try:
        timestamp = datetime.fromisoformat(occurred_at)
    except ValueError as error:
        raise SegmentationError(f"invalid occurred_at for {event_id}") from error
    if timestamp.tzinfo is None:
        raise SegmentationError(f"occurred_at must include timezone for {event_id}")
    payload_value = event.get("payload", {})
    payload = payload_value if isinstance(payload_value, Mapping) else {}
    context_value = payload.get("context", {})
    context = context_value if isinstance(context_value, Mapping) else {}
    application = _first_string(payload, ("application", "app")) or _first_string(
        context, ("process_name", "application", "app")
    )
    document = _first_string(payload, ("document", "document_uri")) or _first_string(
        context, ("document_uri", "document")
    )
    source = str(event.get("source", "")).lower()
    event_type = str(event.get("event_type", "")).lower()
    if "ocr" in source or "ocr" in event_type:
        modality = "ocr"
    elif source in {"input", "user"} or any(token in event_type for token in ("key", "mouse", "input")):
        modality = "input"
    elif source:
        modality = source
    else:
        modality = "unknown"
    return _NormalizedEvent(event_id, session_id, occurred_at, timestamp, application, document, modality)


def _make_episode(
    session_id: str,
    events: Sequence[_NormalizedEvent],
    boundary_reasons: list[str],
    config: SegmentConfig,
) -> SegmentedEpisode:
    event_ids = [event.event_id for event in events]
    episode_id = str(uuid.uuid5(_NAMESPACE, f"{session_id}:{config.fingerprint}:{event_ids[0]}"))
    applications = sorted({event.application for event in events if event.application is not None})
    documents = sorted({event.document for event in events if event.document is not None})
    modalities = sorted({event.modality for event in events})
    return SegmentedEpisode(
        episode_id=episode_id,
        schema_version=SCHEMA_VERSION,
        session_id=session_id,
        start_at=events[0].occurred_at,
        end_at=events[-1].occurred_at,
        event_ids=event_ids,
        context_summary={
            "applications": applications,
            "documents": documents,
            "people": [],
            "topics": [],
            "modalities": modalities,
        },
        boundary_reasons=boundary_reasons,
        embedding_ref=None,
        summary=None,
        hypotheses=[],
        quality={"coherence": 1.0, "confidence": 1.0},
        created_by=CREATED_BY,
    )


def _boundary_positions(
    episodes: Sequence[SegmentedEpisode | Mapping[str, Any]], event_ids: Sequence[str]
) -> set[int]:
    positions: set[int] = set()
    cursor = 0
    for index, episode in enumerate(episodes):
        values = episode.event_ids if isinstance(episode, SegmentedEpisode) else episode.get("event_ids")
        if not isinstance(values, list) or not values:
            raise SegmentationError("episodes must contain non-empty event_ids")
        expected = list(event_ids[cursor : cursor + len(values)])
        if list(values) != expected:
            raise SegmentationError("episodes must cover event_ids contiguously")
        cursor += len(values)
        if index < len(episodes) - 1:
            positions.add(cursor)
    if cursor != len(event_ids):
        raise SegmentationError("episodes do not cover the complete event stream")
    return positions


def _window_diff(predicted: set[int], reference: set[int], event_count: int) -> float:
    if event_count <= 1:
        return 0.0
    window_size = max(1, round(event_count / max(len(reference) + 1, 1)))
    window_count = event_count - window_size
    if window_count <= 0:
        return 0.0
    errors = 0
    for start in range(window_count):
        predicted_count = sum(start < boundary <= start + window_size for boundary in predicted)
        reference_count = sum(start < boundary <= start + window_size for boundary in reference)
        errors += predicted_count != reference_count
    return errors / window_count


def _first_string(mapping: Mapping[str, Any], keys: Sequence[str]) -> str | None:
    for key in keys:
        value = mapping.get(key)
        if isinstance(value, str) and value.strip():
            return value
    return None


def _required_string(mapping: Mapping[str, Any], key: str) -> str:
    value = mapping.get(key)
    if not isinstance(value, str) or not value.strip():
        raise SegmentationError(f"{key} is required")
    return value
