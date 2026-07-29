"""Frozen, replayable longitudinal evaluation reference for SPEC-022."""

from __future__ import annotations

import hashlib
import json
import math
import uuid
from collections.abc import Mapping, Sequence
from dataclasses import dataclass
from datetime import datetime
from itertools import pairwise
from typing import Any

from .schema_validation import validate_shared_schema

SCHEMA_VERSION = "1.0"
BASELINE_POLICY_ID = "chronological_first_snapshot_v0"
CREATED_BY = "longitudinal_evaluation.frozen_snapshots.v1"
CHECKPOINTS = (7, 30, 90)
_NAMESPACE = uuid.UUID("b8af2b4b-f58d-4f98-bf73-468a5f8eaf2d")


class LongitudinalEvaluationError(ValueError):
    """Raised for invalid protocols, snapshots or longitudinal reports."""


@dataclass(frozen=True)
class LongitudinalProtocol:
    protocol_id: str
    schema_version: str
    study_id: str
    metrics: tuple[str, ...]
    acceptance_criteria: dict[str, float]
    holdout_sha256: str
    protocol_hash: str
    frozen: bool

    @classmethod
    def freeze(
        cls,
        *,
        protocol_id: str,
        study_id: str,
        metrics: Sequence[str],
        acceptance_criteria: Mapping[str, float],
        holdout_sha256: str,
    ) -> LongitudinalProtocol:
        _required_name(protocol_id, "protocol_id")
        _required_name(study_id, "study_id")
        normalized_metrics = tuple(sorted({_required_name(metric, "metric") for metric in metrics}))
        if not normalized_metrics:
            raise LongitudinalEvaluationError("at least one metric is required")
        if len(holdout_sha256) != 64 or any(character not in "0123456789abcdef" for character in holdout_sha256):
            raise LongitudinalEvaluationError("holdout_sha256 must be a lowercase SHA-256")
        criteria = _normalize_metrics(acceptance_criteria, "acceptance_criteria")
        payload = {
            "protocol_id": protocol_id,
            "schema_version": SCHEMA_VERSION,
            "study_id": study_id,
            "metrics": list(normalized_metrics),
            "acceptance_criteria": criteria,
            "holdout_sha256": holdout_sha256,
        }
        protocol_hash = hashlib.sha256(_canonical_json(payload)).hexdigest()
        protocol = cls(
            protocol_id,
            SCHEMA_VERSION,
            study_id,
            normalized_metrics,
            criteria,
            holdout_sha256,
            protocol_hash,
            True,
        )
        protocol.to_mapping()
        return protocol

    def to_mapping(self) -> dict[str, Any]:
        value = {
            "protocol_id": self.protocol_id,
            "schema_version": self.schema_version,
            "study_id": self.study_id,
            "metrics": list(self.metrics),
            "acceptance_criteria": dict(self.acceptance_criteria),
            "holdout_sha256": self.holdout_sha256,
            "protocol_hash": self.protocol_hash,
            "frozen": self.frozen,
        }
        validate_shared_schema(value, "longitudinal_protocol.schema.json")
        return value


@dataclass(frozen=True)
class LongitudinalSnapshot:
    snapshot_id: str
    schema_version: str
    study_id: str
    checkpoint_day: int
    captured_at: str
    protocol_hash: str
    cognitive_metrics: dict[str, float]
    operational_metrics: dict[str, float]
    self_model_version: int
    self_model_digest: str
    source_refs: tuple[str, ...]

    def to_mapping(self) -> dict[str, Any]:
        value = {
            "snapshot_id": self.snapshot_id,
            "schema_version": self.schema_version,
            "study_id": self.study_id,
            "checkpoint_day": self.checkpoint_day,
            "captured_at": self.captured_at,
            "protocol_hash": self.protocol_hash,
            "cognitive_metrics": dict(self.cognitive_metrics),
            "operational_metrics": dict(self.operational_metrics),
            "self_model_version": self.self_model_version,
            "self_model_digest": self.self_model_digest,
            "source_refs": list(self.source_refs),
        }
        validate_shared_schema(value, "longitudinal_snapshot.schema.json")
        return value


@dataclass(frozen=True)
class LongitudinalReport:
    report_id: str
    schema_version: str
    study_id: str
    protocol_hash: str
    baseline_policy_id: str
    snapshots: tuple[LongitudinalSnapshot, ...]
    retention_curve: tuple[dict[str, Any], ...]
    calibration: dict[str, Any]
    change_report: dict[str, Any]
    self_model_drift: dict[str, Any]

    def to_mapping(self) -> dict[str, Any]:
        value = {
            "report_id": self.report_id,
            "schema_version": self.schema_version,
            "study_id": self.study_id,
            "protocol_hash": self.protocol_hash,
            "baseline_policy_id": self.baseline_policy_id,
            "snapshots": [snapshot.to_mapping() for snapshot in self.snapshots],
            "retention_curve": [dict(point) for point in self.retention_curve],
            "calibration": dict(self.calibration),
            "change_report": _copy_json(self.change_report),
            "self_model_drift": _copy_json(self.self_model_drift),
        }
        validate_shared_schema(value, "longitudinal_report.schema.json")
        return value


class LongitudinalEvaluator:
    """Collect three immutable checkpoints and derive a deterministic report."""

    def __init__(self, protocol: LongitudinalProtocol) -> None:
        if not protocol.frozen:
            raise LongitudinalEvaluationError("longitudinal protocol must be frozen")
        protocol.to_mapping()
        self.protocol = protocol
        self._snapshots: dict[int, LongitudinalSnapshot] = {}

    def record_snapshot(
        self,
        *,
        checkpoint_day: int,
        captured_at: str,
        cognitive_metrics: Mapping[str, float],
        operational_metrics: Mapping[str, float],
        self_model_version: int,
        self_model_digest: str,
        source_refs: Sequence[str],
    ) -> LongitudinalSnapshot:
        if checkpoint_day not in CHECKPOINTS:
            raise LongitudinalEvaluationError("checkpoint_day must be 7, 30 or 90")
        if checkpoint_day in self._snapshots:
            raise LongitudinalEvaluationError("checkpoint already recorded")
        _parse_time(captured_at)
        if self_model_version < 1:
            raise LongitudinalEvaluationError("self_model_version must be positive")
        _required_name(self_model_digest, "self_model_digest")
        refs = tuple(_required_name(reference, "source_ref") for reference in source_refs)
        if not refs:
            raise LongitudinalEvaluationError("at least one source reference is required")
        snapshot_id = str(uuid.uuid5(_NAMESPACE, f"{self.protocol.study_id}:{self.protocol.protocol_hash}:{checkpoint_day}"))
        snapshot = LongitudinalSnapshot(
            snapshot_id=snapshot_id,
            schema_version=SCHEMA_VERSION,
            study_id=self.protocol.study_id,
            checkpoint_day=checkpoint_day,
            captured_at=captured_at,
            protocol_hash=self.protocol.protocol_hash,
            cognitive_metrics=_normalize_metrics(cognitive_metrics, "cognitive_metrics"),
            operational_metrics=_normalize_metrics(operational_metrics, "operational_metrics"),
            self_model_version=self_model_version,
            self_model_digest=self_model_digest,
            source_refs=refs,
        )
        snapshot.to_mapping()
        self._snapshots[checkpoint_day] = snapshot
        return snapshot

    def snapshots(self) -> tuple[LongitudinalSnapshot, ...]:
        return tuple(self._snapshots[day] for day in sorted(self._snapshots))

    def report(self) -> LongitudinalReport:
        snapshots = self.snapshots()
        if not snapshots:
            raise LongitudinalEvaluationError("at least one snapshot is required")
        snapshot_ids = ":".join(snapshot.snapshot_id for snapshot in snapshots)
        report_id = str(uuid.uuid5(_NAMESPACE, f"report:{self.protocol.protocol_hash}:{snapshot_ids}"))
        return LongitudinalReport(
            report_id=report_id,
            schema_version=SCHEMA_VERSION,
            study_id=self.protocol.study_id,
            protocol_hash=self.protocol.protocol_hash,
            baseline_policy_id=BASELINE_POLICY_ID,
            snapshots=snapshots,
            retention_curve=tuple(_retention_curve(snapshots)),
            calibration=_series(snapshots, "calibration_ece"),
            change_report={
                "cognitive": _change_report(snapshots, "cognitive_metrics"),
                "operational": _change_report(snapshots, "operational_metrics"),
            },
            self_model_drift=_self_model_drift(snapshots),
        )

    @classmethod
    def replay(
        cls,
        protocol: LongitudinalProtocol,
        snapshots: Sequence[LongitudinalSnapshot],
    ) -> LongitudinalReport:
        evaluator = cls(protocol)
        for snapshot in snapshots:
            if snapshot.to_mapping()["protocol_hash"] != protocol.protocol_hash:
                raise LongitudinalEvaluationError("snapshot belongs to another protocol")
            recorded = evaluator.record_snapshot(
                checkpoint_day=snapshot.checkpoint_day,
                captured_at=snapshot.captured_at,
                cognitive_metrics=snapshot.cognitive_metrics,
                operational_metrics=snapshot.operational_metrics,
                self_model_version=snapshot.self_model_version,
                self_model_digest=snapshot.self_model_digest,
                source_refs=snapshot.source_refs,
            )
            if recorded.snapshot_id != snapshot.snapshot_id:
                raise LongitudinalEvaluationError("snapshot identity is not deterministic")
        return evaluator.report()


def _retention_curve(snapshots: Sequence[LongitudinalSnapshot]) -> list[dict[str, Any]]:
    return [
        {"checkpoint_day": snapshot.checkpoint_day, "value": snapshot.cognitive_metrics["retention_score"]}
        for snapshot in snapshots
        if "retention_score" in snapshot.cognitive_metrics
    ]


def _series(snapshots: Sequence[LongitudinalSnapshot], metric: str) -> dict[str, Any]:
    values = [
        {"checkpoint_day": snapshot.checkpoint_day, "value": snapshot.cognitive_metrics[metric]}
        for snapshot in snapshots
        if metric in snapshot.cognitive_metrics
    ]
    return {"metric": metric, "observations": values, "observed_count": len(values)}


def _change_report(snapshots: Sequence[LongitudinalSnapshot], category: str) -> dict[str, Any]:
    metrics = sorted({metric for snapshot in snapshots for metric in getattr(snapshot, category)})
    report: dict[str, Any] = {}
    for metric in metrics:
        values = [
            (snapshot.checkpoint_day, getattr(snapshot, category)[metric])
            for snapshot in snapshots
            if metric in getattr(snapshot, category)
        ]
        first_day, first = values[0]
        last_day, last = values[-1]
        delta = last - first
        report[metric] = {
            "first_checkpoint_day": first_day,
            "last_checkpoint_day": last_day,
            "first": first,
            "last": last,
            "delta": delta,
            "status": "gain" if delta > 0 else "loss" if delta < 0 else "stable",
            "observed_count": len(values),
        }
    return report


def _self_model_drift(snapshots: Sequence[LongitudinalSnapshot]) -> dict[str, Any]:
    versions = [snapshot.self_model_version for snapshot in snapshots]
    digests = [snapshot.self_model_digest for snapshot in snapshots]
    return {
        "versions": versions,
        "first_version": versions[0],
        "last_version": versions[-1],
        "version_delta": versions[-1] - versions[0],
        "digest_change_count": sum(first != second for first, second in pairwise(digests)),
        "changed": any(first != second for first, second in pairwise(digests)),
    }


def _normalize_metrics(values: Mapping[str, float], field_name: str) -> dict[str, float]:
    result: dict[str, float] = {}
    for name, value in values.items():
        _required_name(name, f"{field_name} name")
        if isinstance(value, bool) or not math.isfinite(float(value)):
            raise LongitudinalEvaluationError(f"{field_name} values must be finite numbers")
        result[name] = float(value)
    return dict(sorted(result.items()))


def _required_name(value: str, field_name: str) -> str:
    if not isinstance(value, str) or not value.strip():
        raise LongitudinalEvaluationError(f"{field_name} must be a non-empty string")
    return value.strip()


def _parse_time(value: str) -> datetime:
    try:
        parsed = datetime.fromisoformat(value)
    except ValueError as error:
        raise LongitudinalEvaluationError(f"invalid timestamp: {value}") from error
    if parsed.tzinfo is None:
        raise LongitudinalEvaluationError("timestamps must include timezone")
    return parsed


def _canonical_json(value: Mapping[str, Any]) -> bytes:
    return json.dumps(value, ensure_ascii=False, sort_keys=True, separators=(",", ":")).encode("utf-8")


def _copy_json(value: Any) -> Any:
    if isinstance(value, Mapping):
        return {str(key): _copy_json(item) for key, item in value.items()}
    if isinstance(value, list):
        return [_copy_json(item) for item in value]
    return value
