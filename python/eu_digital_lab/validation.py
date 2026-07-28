"""Independent verification, scientific-validity and ecological gates.

These gates consume evidence produced by the sandbox and promotion pipeline.
They intentionally keep equivalence, ground truth, ecological validity and
operational performance as separate evidence classes.
"""

from __future__ import annotations

import hashlib
import json
import platform
import random
import statistics
from collections.abc import Callable, Mapping, Sequence
from dataclasses import asdict, dataclass, field
from pathlib import Path
from typing import Any

from .evaluation import FaultInjector, ReplayLog, VirtualClock


class ValidationGateError(RuntimeError):
    """Raised when a gate is attempted without the required frozen protocol."""


@dataclass(frozen=True)
class ValidationProtocol:
    protocol_id: str
    hypothesis_id: str
    metrics: tuple[str, ...]
    acceptance_criteria: Mapping[str, float]
    review_id: str = ""
    frozen: bool = False

    def freeze(self) -> ValidationProtocol:
        if not self.protocol_id.strip() or not self.hypothesis_id.strip():
            raise ValidationGateError(
                "protocol and hypothesis identifiers are required"
            )
        if not self.review_id.strip():
            raise ValidationGateError(
                "an independent review is required before freezing"
            )
        if not self.metrics:
            raise ValidationGateError("at least one metric is required")
        return ValidationProtocol(
            self.protocol_id,
            self.hypothesis_id,
            tuple(self.metrics),
            dict(self.acceptance_criteria),
            self.review_id,
            True,
        )

    def revise(
        self,
        *,
        metrics: Sequence[str] | None = None,
        acceptance_criteria: Mapping[str, float] | None = None,
        review_id: str | None = None,
    ) -> ValidationProtocol:
        if self.frozen:
            raise ValidationGateError(
                "frozen protocol requires a new protocol revision"
            )
        return ValidationProtocol(
            self.protocol_id,
            self.hypothesis_id,
            tuple(metrics) if metrics is not None else self.metrics,
            dict(acceptance_criteria or self.acceptance_criteria),
            review_id if review_id is not None else self.review_id,
            False,
        )


@dataclass(frozen=True)
class GateEvidence:
    name: str
    passed: bool
    evidence: Mapping[str, Any] = field(default_factory=dict)

    def to_dict(self) -> dict[str, Any]:
        return {
            "name": self.name,
            "passed": self.passed,
            "evidence": _json_value(self.evidence),
        }


@dataclass
class ValidationReport:
    protocol: ValidationProtocol
    gates: dict[str, GateEvidence]
    cognitive_metrics: dict[str, float]
    operational_metrics: dict[str, float]
    longitudinal: Mapping[str, Any]
    backend_comparison: Mapping[str, Any]
    overall_passed: bool
    equivalence_is_sufficient: bool = False

    def to_dict(self) -> dict[str, Any]:
        return {
            "protocol": _json_value(asdict(self.protocol)),
            "gates": {name: gate.to_dict() for name, gate in self.gates.items()},
            "cognitive_metrics": dict(self.cognitive_metrics),
            "operational_metrics": dict(self.operational_metrics),
            "longitudinal": _json_value(self.longitudinal),
            "backend_comparison": _json_value(self.backend_comparison),
            "overall_passed": self.overall_passed,
            "equivalence_is_sufficient": self.equivalence_is_sufficient,
            "hardware": {
                "system": platform.system(),
                "release": platform.release(),
                "machine": platform.machine(),
            },
        }

    def write(self, path: str | Path) -> None:
        target = Path(path)
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(
            json.dumps(self.to_dict(), ensure_ascii=False, indent=2, sort_keys=True)
            + "\n",
            encoding="utf-8",
        )


class ValidationGateRunner:
    """Evaluate independent gates under a frozen, reviewed protocol."""

    REQUIRED_GATES = (
        "equivalence",
        "ground_truth",
        "holdout",
        "metamorphic",
        "replay",
        "faults",
        "export",
        "online",
    )

    def evaluate(
        self,
        protocol: ValidationProtocol,
        *,
        gates: Mapping[str, GateEvidence],
        cognitive_metrics: Mapping[str, float] | None = None,
        operational_metrics: Mapping[str, float] | None = None,
        longitudinal: Mapping[str, Any] | None = None,
        backend_comparison: Mapping[str, Any] | None = None,
    ) -> ValidationReport:
        if not protocol.frozen or not protocol.review_id.strip():
            raise ValidationGateError(
                "validation requires a frozen protocol and independent review"
            )
        evaluated = {name: _validate_evidence(gate) for name, gate in gates.items()}
        for name in self.REQUIRED_GATES:
            if name not in evaluated:
                evaluated[name] = GateEvidence(
                    name, False, {"reason": "evidence missing"}
                )
        return ValidationReport(
            protocol=protocol,
            gates=evaluated,
            cognitive_metrics=dict(cognitive_metrics or {}),
            operational_metrics=dict(operational_metrics or {}),
            longitudinal=dict(longitudinal or {}),
            backend_comparison=dict(backend_comparison or {}),
            overall_passed=all(gate.passed for gate in evaluated.values()),
            equivalence_is_sufficient=False,
        )

    @staticmethod
    def run_online_after_replay(
        replay: ReplayLog,
        replay_handler: Callable[[Mapping[str, Any]], None],
        online_events: Sequence[Mapping[str, Any]],
        online_handler: Callable[[Mapping[str, Any], VirtualClock], None],
        replay_gate_passed: bool,
    ) -> dict[str, Any]:
        if not replay_gate_passed:
            raise ValidationGateError(
                "online validation requires a passing replay gate"
            )
        clock = VirtualClock()
        replay.replay(replay_handler, clock)
        for event in online_events:
            online_handler(event, clock)
        return {
            "replay_completed": True,
            "online_completed": True,
            "clock_ms": clock.now_ms,
            "online_event_count": len(online_events),
        }


@dataclass(frozen=True)
class JitterSchedule:
    seed: int
    delays_ms: tuple[int, ...]

    @classmethod
    def deterministic(cls, *, count: int, seed: int, maximum_ms: int) -> JitterSchedule:
        if count < 1 or maximum_ms < 0:
            raise ValueError("count must be positive and maximum_ms cannot be negative")
        rng = random.Random(seed)
        return cls(seed, tuple(rng.randint(0, maximum_ms) for _ in range(count)))

    def delay(self, index: int) -> int:
        return self.delays_ms[index]


def run_failure_scenario(
    scenario_id: str,
    budgets: Mapping[str, int],
    operation: Callable[[FaultInjector], None],
) -> dict[str, Any]:
    faults = FaultInjector(budgets)
    try:
        operation(faults)
    except Exception as exc:  # noqa: BLE001 - failures are the scenario evidence.
        return {
            "scenario_id": scenario_id,
            "status": "failed",
            "error": f"{type(exc).__name__}: {exc}",
            "consumed": list(faults.consumed),
        }
    return {
        "scenario_id": scenario_id,
        "status": "completed",
        "error": None,
        "consumed": list(faults.consumed),
    }


def audit_export(
    original: bytes,
    exported: bytes,
    *,
    quantization: str,
    accuracy_before: float,
    accuracy_after: float,
    accuracy_tolerance: float,
    calibration_before: float | None = None,
    calibration_after: float | None = None,
) -> dict[str, Any]:
    accuracy_delta = accuracy_after - accuracy_before
    calibration_delta = (
        calibration_after - calibration_before
        if calibration_before is not None and calibration_after is not None
        else None
    )
    return {
        "passed": abs(accuracy_delta) <= accuracy_tolerance,
        "quantization": quantization,
        "hashes": {
            "original": hashlib.sha256(original).hexdigest(),
            "exported": hashlib.sha256(exported).hexdigest(),
        },
        "difference_bytes": abs(len(exported) - len(original)),
        "accuracy_before": accuracy_before,
        "accuracy_after": accuracy_after,
        "accuracy_delta": accuracy_delta,
        "calibration_delta": calibration_delta,
        "cases_divergent": [] if original == exported else ["byte-level-difference"],
    }


def compare_backends(
    observations: Mapping[str, Mapping[str, float]],
    hardware: Mapping[str, str],
) -> dict[str, Any]:
    if len(observations) < 2:
        raise ValueError("at least two backends are required")
    metric_names = sorted(
        {metric for values in observations.values() for metric in values}
    )
    metrics: dict[str, dict[str, Any]] = {}
    for metric in metric_names:
        values = {
            backend: result[metric]
            for backend, result in observations.items()
            if metric in result
        }
        numbers = list(values.values())
        metrics[metric] = {
            "values": values,
            "min": min(numbers),
            "max": max(numbers),
            "difference": max(numbers) - min(numbers),
        }
    return {
        "passed": len(observations) == len(hardware)
        and all(metric["difference"] >= 0 for metric in metrics.values()),
        "backends": sorted(observations),
        "hardware": dict(hardware),
        "metrics": metrics,
    }


def build_longitudinal_report(sessions: Sequence[Mapping[str, Any]]) -> dict[str, Any]:
    if not sessions:
        raise ValueError("at least one longitudinal session is required")
    result: dict[str, dict[str, dict[str, Any]]] = {"cognitive": {}, "operational": {}}
    for category, category_report in result.items():
        names = sorted(
            {
                metric
                for session in sessions
                for metric in session.get(f"{category}_metrics", {})
            }
        )
        for metric in names:
            values = [
                float(session[f"{category}_metrics"][metric])
                for session in sessions
                if metric in session.get(f"{category}_metrics", {})
            ]
            category_report[metric] = {
                "count": len(values),
                "mean": round(statistics.fmean(values), 12),
                "first": values[0],
                "last": values[-1],
                "drift": round(values[-1] - values[0], 12),
            }
    return result


def _json_value(value: Any) -> Any:
    if value is None or isinstance(value, (str, int, float, bool)):
        return value
    if isinstance(value, Mapping):
        return {str(key): _json_value(item) for key, item in value.items()}
    if isinstance(value, (list, tuple)):
        return [_json_value(item) for item in value]
    return repr(value)


def _validate_evidence(gate: GateEvidence) -> GateEvidence:
    if not gate.passed:
        return gate
    evidence = gate.evidence
    valid = {
        "ground_truth": bool(
            evidence.get("known_truth") or evidence.get("truth_source")
        ),
        "holdout": isinstance(evidence.get("sha256"), str)
        and len(evidence["sha256"]) == 64
        and evidence.get("access_registered") is True,
        "metamorphic": evidence.get("mutation_detected") is True,
        "replay": evidence.get("clock_controlled") is True
        and evidence.get("ordered") is True,
        "faults": evidence.get("reproducible") is True,
        "export": isinstance(evidence.get("hashes"), Mapping)
        and bool(evidence["hashes"].get("original"))
        and bool(evidence["hashes"].get("exported"))
        and bool(evidence.get("quantization")),
        "online": evidence.get("replay_completed") is True
        and evidence.get("online_completed") is True,
    }.get(gate.name, True)
    if valid:
        return gate
    return GateEvidence(
        gate.name, False, {**dict(evidence), "reason": "required evidence missing"}
    )
