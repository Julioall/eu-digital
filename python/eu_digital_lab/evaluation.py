"""Local, reproducible experiment primitives for SPEC-018.

This module is deliberately dependency-free.  It records engineering evidence
without presenting runtime performance or Python/C++ agreement as cognitive
validity.
"""

from __future__ import annotations

import hashlib
import json
import platform
import statistics
import subprocess
import time
import tracemalloc
from collections.abc import Callable, Mapping, Sequence
from dataclasses import asdict, dataclass, field
from pathlib import Path
from typing import Any, TypeAlias

MetricValue: TypeAlias = float | int | Sequence[float | int]  # noqa: UP040
ExperimentFunction: TypeAlias = Callable[[Any, "ExperimentContext"], Any]  # noqa: UP040
MetricFunction: TypeAlias = Callable[[Any, Any], Mapping[str, MetricValue]]  # noqa: UP040
OperationalMetricFunction: TypeAlias = Callable[[Any], Mapping[str, MetricValue]]  # noqa: UP040


class EvaluationError(RuntimeError):
    """Base error for invalid or incomplete evaluation runs."""


class HoldoutAccessError(EvaluationError):
    """Raised when a locked holdout is accessed outside a final evaluation."""


class InjectedFault(RuntimeError):
    """Raised by an explicitly configured fault injection point."""


class MetamorphicFailure(EvaluationError):
    """Raised when a metamorphic relation cannot be evaluated."""


@dataclass(frozen=True)
class ExperimentConfig:
    experiment_id: str
    hypothesis: str
    baseline_name: str = "baseline"
    treatment_name: str = "treatment"
    seed: int = 0
    backend: str = "reference-python"
    feature_flags: Mapping[str, bool] = field(default_factory=dict)
    nondeterminism_sources: tuple[str, ...] = ()
    configuration: Mapping[str, Any] = field(default_factory=dict)
    repo_root: Path | None = None

    def __post_init__(self) -> None:
        if not self.experiment_id.strip():
            raise ValueError("experiment_id cannot be empty")
        if not self.hypothesis.strip():
            raise ValueError("hypothesis cannot be empty")
        if not self.baseline_name.strip() or not self.treatment_name.strip():
            raise ValueError("condition names cannot be empty")
        if any(not isinstance(value, bool) for value in self.feature_flags.values()):
            raise ValueError("feature flags must be booleans")

    def to_dict(self) -> dict[str, Any]:
        value = asdict(self)
        value["repo_root"] = str(self.repo_root) if self.repo_root is not None else None
        value["feature_flags"] = dict(self.feature_flags)
        value["configuration"] = dict(self.configuration)
        return value


@dataclass(frozen=True)
class MetricSummary:
    count: int
    mean: float
    standard_deviation: float
    confidence_interval_95: tuple[float, float]

    def to_dict(self) -> dict[str, Any]:
        value = asdict(self)
        value["confidence_interval_95"] = list(self.confidence_interval_95)
        return value


def summarize_metric(value: MetricValue) -> MetricSummary:
    if isinstance(value, bool):
        raise TypeError("boolean is not a metric")
    if isinstance(value, (int, float)):
        samples = [float(value)]
    else:
        samples = [float(item) for item in value]
    if not samples:
        raise ValueError("metric must have at least one sample")
    mean = statistics.fmean(samples)
    deviation = statistics.stdev(samples) if len(samples) > 1 else 0.0
    margin = 1.96 * deviation / (len(samples) ** 0.5) if len(samples) > 1 else 0.0
    return MetricSummary(len(samples), mean, deviation, (mean - margin, mean + margin))


@dataclass
class VirtualClock:
    """Deterministic clock used by replayable experiments."""

    now_ms: int = 0

    def advance(self, milliseconds: int) -> None:
        if milliseconds < 0:
            raise ValueError("clock cannot move backwards")
        self.now_ms += milliseconds


@dataclass(frozen=True)
class ReplayRecord:
    at_ms: int
    event: Mapping[str, Any]


class ReplayLog:
    """Ordered event log with a virtual-clock replay operation."""

    def __init__(self) -> None:
        self._records: list[ReplayRecord] = []

    @property
    def records(self) -> tuple[ReplayRecord, ...]:
        return tuple(self._records)

    def record(self, event: Mapping[str, Any], *, at_ms: int) -> None:
        if at_ms < 0 or (self._records and at_ms < self._records[-1].at_ms):
            raise ValueError("replay timestamps must be non-negative and ordered")
        self._records.append(ReplayRecord(at_ms, dict(event)))

    def replay(
        self, handler: Callable[[Mapping[str, Any]], None], clock: VirtualClock
    ) -> None:
        for record in self._records:
            clock.advance(record.at_ms - clock.now_ms)
            handler(record.event)


class FaultInjector:
    """Finite, named fault budget for deterministic failure experiments."""

    def __init__(self, budgets: Mapping[str, int] | None = None) -> None:
        self._budgets = dict(budgets or {})
        if any(count < 0 for count in self._budgets.values()):
            raise ValueError("fault budgets cannot be negative")
        self._consumed: list[str] = []

    @property
    def consumed(self) -> tuple[str, ...]:
        return tuple(self._consumed)

    def check(self, name: str) -> None:
        remaining = self._budgets.get(name, 0)
        if remaining <= 0:
            return
        self._budgets[name] = remaining - 1
        self._consumed.append(name)
        raise InjectedFault(f"injected fault: {name}")


@dataclass
class ExperimentContext:
    condition: str
    seed: int
    feature_flags: Mapping[str, bool]
    clock: VirtualClock
    faults: FaultInjector

    def module_enabled(self, module: str) -> bool:
        return self.feature_flags.get(module, True)


@dataclass
class TrialResult:
    condition: str
    status: str
    output: Any = None
    error: str | None = None
    cognitive_metrics: dict[str, MetricSummary] = field(default_factory=dict)
    operational_metrics: dict[str, MetricSummary] = field(default_factory=dict)
    ground_truth: dict[str, MetricSummary] = field(default_factory=dict)
    resources: dict[str, float] = field(default_factory=dict)

    def to_dict(self) -> dict[str, Any]:
        return {
            "condition": self.condition,
            "status": self.status,
            "output": _json_value(self.output),
            "error": self.error,
            "cognitive_metrics": {
                key: value.to_dict() for key, value in self.cognitive_metrics.items()
            },
            "operational_metrics": {
                key: value.to_dict() for key, value in self.operational_metrics.items()
            },
            "ground_truth": {
                key: value.to_dict() for key, value in self.ground_truth.items()
            },
            "resources": dict(self.resources),
        }


@dataclass
class ExperimentReport:
    experiment_id: str
    hypothesis: str
    configuration: dict[str, Any]
    provenance: dict[str, Any]
    trials: dict[str, TrialResult]
    comparisons: dict[str, dict[str, dict[str, float | None]]]
    divergences: list[str]

    def to_dict(self) -> dict[str, Any]:
        return {
            "experiment_id": self.experiment_id,
            "hypothesis": self.hypothesis,
            "configuration": _json_value(self.configuration),
            "provenance": _json_value(self.provenance),
            "trials": {key: value.to_dict() for key, value in self.trials.items()},
            "comparisons": self.comparisons,
            "divergences": list(self.divergences),
            "metric_policy": {
                "cognitive": "reported separately from operational metrics",
                "operational": "engineering evidence only; not cognitive validity",
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


class ExperimentRunner:
    """Run baseline and treatment with the same seed and feature flags."""

    def __init__(
        self, *, clock: VirtualClock | None = None, faults: FaultInjector | None = None
    ) -> None:
        self.clock = clock or VirtualClock()
        self.faults = faults or FaultInjector()

    def run(
        self,
        config: ExperimentConfig,
        input_data: Any,
        *,
        baseline: ExperimentFunction,
        treatment: ExperimentFunction,
        ground_truth: Any = None,
        cognitive_metrics: MetricFunction | None = None,
        operational_metrics: OperationalMetricFunction | None = None,
        ground_truth_metrics: MetricFunction | None = None,
    ) -> ExperimentReport:
        trials = {
            config.baseline_name: self._run_trial(
                config.baseline_name,
                config,
                input_data,
                baseline,
                ground_truth,
                cognitive_metrics,
                operational_metrics,
                ground_truth_metrics,
            ),
            config.treatment_name: self._run_trial(
                config.treatment_name,
                config,
                input_data,
                treatment,
                ground_truth,
                cognitive_metrics,
                operational_metrics,
                ground_truth_metrics,
            ),
        }
        return ExperimentReport(
            experiment_id=config.experiment_id,
            hypothesis=config.hypothesis,
            configuration={
                **config.to_dict(),
                "feature_flags": dict(config.feature_flags),
            },
            provenance=_provenance(config),
            trials=trials,
            comparisons={
                "cognitive": _compare(
                    trials,
                    config.baseline_name,
                    config.treatment_name,
                    "cognitive_metrics",
                ),
                "operational": _compare(
                    trials,
                    config.baseline_name,
                    config.treatment_name,
                    "operational_metrics",
                ),
            },
            divergences=_divergences(trials),
        )

    def _run_trial(
        self,
        condition: str,
        config: ExperimentConfig,
        input_data: Any,
        operation: ExperimentFunction,
        ground_truth: Any,
        cognitive_metrics: MetricFunction | None,
        operational_metrics: OperationalMetricFunction | None,
        ground_truth_metrics: MetricFunction | None,
    ) -> TrialResult:
        context = ExperimentContext(
            condition,
            config.seed,
            config.feature_flags,
            VirtualClock(self.clock.now_ms),
            self.faults,
        )
        tracemalloc.start()
        started = time.perf_counter_ns()
        try:
            output = operation(input_data, context)
            cognitive = _summarize_metrics(
                cognitive_metrics(output, ground_truth) if cognitive_metrics else {}
            )
            operational = _summarize_metrics(
                operational_metrics(output) if operational_metrics else {}
            )
            truth = _summarize_metrics(
                ground_truth_metrics(output, ground_truth)
                if ground_truth_metrics
                else {}
            )
            status = "completed"
            error = None
        except Exception as exc:  # noqa: BLE001 - experiment failures are evidence in the report.
            output = None
            cognitive = {}
            operational = {}
            truth = {}
            status = "failed"
            error = f"{type(exc).__name__}: {exc}"
        finally:
            _, peak_memory = tracemalloc.get_traced_memory()
            tracemalloc.stop()
        elapsed = (time.perf_counter_ns() - started) / 1_000_000
        return TrialResult(
            condition=condition,
            status=status,
            output=output,
            error=error,
            cognitive_metrics=cognitive,
            operational_metrics=operational,
            ground_truth=truth,
            resources={"duration_ms": elapsed, "peak_memory_bytes": float(peak_memory)},
        )


@dataclass(frozen=True)
class MetamorphicResult:
    name: str
    passed: bool
    violation_detected: bool


def run_metamorphic_test(
    value: Any,
    *,
    transform: Callable[[Any], Any],
    system: Callable[[Any], Any],
    relation: Callable[[Any, Any], bool],
    name: str,
) -> MetamorphicResult:
    if not name.strip():
        raise MetamorphicFailure("metamorphic relation name cannot be empty")
    original = system(value)
    transformed = system(transform(value))
    passed = relation(original, transformed)
    return MetamorphicResult(name, passed, not passed)


class DatasetRepository:
    """Manifest-backed dataset access with a locked holdout policy."""

    def __init__(self, root: str | Path, *, holdout_split: str = "test") -> None:
        self.root = Path(root)
        self.manifest = json.loads(
            (self.root / "manifest.json").read_text(encoding="utf-8")
        )
        self.holdout_split = holdout_split
        if self.manifest.get("holdout_split") != holdout_split:
            raise EvaluationError(
                "manifest holdout split does not match repository policy"
            )
        self._access_log: list[dict[str, str]] = []

    @property
    def access_log(self) -> tuple[dict[str, str], ...]:
        return tuple(self._access_log)

    def metadata(self, split: str) -> dict[str, Any]:
        entries = self._entries(split)
        return {
            "split": split,
            "locked": split == self.holdout_split,
            "policy": "locked-until-final-evaluation"
            if split == self.holdout_split
            else "development-access",
            "files": [
                {"path": item["path"], "sha256": item["sha256"]} for item in entries
            ],
        }

    def load(self, split: str, *, purpose: str | None = None) -> list[dict[str, Any]]:
        if split == self.holdout_split and purpose != "final-evaluation":
            raise HoldoutAccessError(
                "holdout is locked; purpose must be final-evaluation"
            )
        loaded: list[dict[str, Any]] = []
        for item in self._entries(split):
            path = self.root / item["path"]
            content = path.read_bytes()
            digest = hashlib.sha256(content).hexdigest()
            if digest != item["sha256"]:
                raise EvaluationError(f"dataset hash mismatch: {item['path']}")
            loaded.append(json.loads(content.decode("utf-8")))
        if split == self.holdout_split:
            self._access_log.append({"split": split, "purpose": purpose or ""})
        return loaded

    def _entries(self, split: str) -> list[dict[str, Any]]:
        try:
            entries = self.manifest["splits"][split]
        except KeyError as exc:
            raise EvaluationError(f"unknown dataset split: {split}") from exc
        if not isinstance(entries, list) or not entries:
            raise EvaluationError(f"dataset split is empty: {split}")
        return entries


def _summarize_metrics(metrics: Mapping[str, MetricValue]) -> dict[str, MetricSummary]:
    return {name: summarize_metric(value) for name, value in metrics.items()}


def _compare(
    trials: Mapping[str, TrialResult], baseline: str, treatment: str, field_name: str
) -> dict[str, dict[str, float | None]]:
    first = getattr(trials[baseline], field_name)
    second = getattr(trials[treatment], field_name)
    result: dict[str, dict[str, float | None]] = {}
    for name in sorted(set(first) | set(second)):
        first_mean = first[name].mean if name in first else None
        second_mean = second[name].mean if name in second else None
        result[name] = {
            "baseline": first_mean,
            "treatment": second_mean,
            "difference": second_mean - first_mean
            if first_mean is not None and second_mean is not None
            else None,
        }
    return result


def _divergences(trials: Mapping[str, TrialResult]) -> list[str]:
    return [
        f"{name}: {trial.error}"
        for name, trial in trials.items()
        if trial.status != "completed" and trial.error
    ]


def _provenance(config: ExperimentConfig) -> dict[str, Any]:
    commit = "unknown"
    if config.repo_root is not None:
        completed = subprocess.run(
            ["git", "rev-parse", "HEAD"],
            cwd=config.repo_root,
            capture_output=True,
            text=True,
            check=False,
        )
        if completed.returncode == 0:
            commit = completed.stdout.strip()
    return {
        "commit": commit,
        "hardware": {
            "system": platform.system(),
            "release": platform.release(),
            "machine": platform.machine(),
            "processor": platform.processor() or "unknown",
        },
        "backend": config.backend,
        "nondeterminism_sources": list(config.nondeterminism_sources),
    }


def _json_value(value: Any) -> Any:
    if value is None or isinstance(value, (str, int, float, bool)):
        return value
    if isinstance(value, Path):
        return str(value)
    if isinstance(value, Mapping):
        return {str(key): _json_value(item) for key, item in value.items()}
    if isinstance(value, (list, tuple)):
        return [_json_value(item) for item in value]
    return repr(value)
