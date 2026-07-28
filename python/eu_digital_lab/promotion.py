"""Python side of the reproducible Python-to-C++ promotion pipeline.

The pipeline transports canonical fixture bytes to both runners and compares
their semantic JSON outputs.  It records divergence and performance evidence,
but never treats cross-language agreement as scientific ground truth.
"""

from __future__ import annotations

import hashlib
import json
import math
import platform
import subprocess
from collections.abc import Callable, Mapping, Sequence
from dataclasses import dataclass
from pathlib import Path
from typing import Any

Runner = Callable[[bytes], bytes]
_STATUSES = {
    "draft",
    "reference_frozen",
    "candidate_ready",
    "validated",
    "rejected",
    "promoted",
}
_REQUIRED_TOP_LEVEL = {
    "promotion_id",
    "component_id",
    "component_version",
    "hypothesis",
    "reference",
    "candidate",
    "contract",
    "dataset",
    "equivalence",
    "performance",
    "status",
}
_REQUIRED_NESTED = {
    "hypothesis": {"id", "report_uri"},
    "reference": {
        "language",
        "package",
        "commit",
        "entrypoint",
        "environment_lock_hash",
    },
    "candidate": {"language", "target", "commit", "compiler", "build_profile"},
    "contract": {
        "input_schema",
        "output_schema",
        "state_schema",
        "error_schema",
        "clock_semantics",
        "random_seed_policy",
    },
    "dataset": {"fixture_set", "hash", "case_count"},
    "equivalence": {
        "type",
        "absolute_tolerance",
        "relative_tolerance",
        "invariants",
        "acceptance_metrics",
    },
    "performance": {
        "baseline_hardware",
        "maximum_latency_ms",
        "maximum_memory_mb",
        "minimum_throughput",
    },
}


class PromotionError(RuntimeError):
    """Base error for invalid promotion records or runs."""


class PromotionGateError(PromotionError):
    """Raised when a component has no approved promotion."""


def canonical_json_bytes(value: Any) -> bytes:
    """Serialize JSON deterministically for the cross-language boundary."""

    return (
        json.dumps(value, ensure_ascii=False, sort_keys=True, separators=(",", ":"))
        + "\n"
    ).encode("utf-8")


def write_fixture_set(path: str | Path, cases: Sequence[Any]) -> bytes:
    """Write a canonical JSON-lines fixture set and return its bytes."""

    values = b"".join(canonical_json_bytes(case) for case in cases)
    if not values:
        raise ValueError("fixture set cannot be empty")
    target = Path(path)
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_bytes(values)
    return values


def python_runner(transform: Callable[[Any], Any]) -> Runner:
    """Build a deterministic Python reference runner over JSON-lines cases."""

    def run(fixture_bytes: bytes) -> bytes:
        return b"".join(
            canonical_json_bytes(transform(case)) for case in _json_lines(fixture_bytes)
        )

    return run


def command_runner(command: Sequence[str]) -> Runner:
    """Adapt a local C++ (or other native) runner to the pipeline port."""

    command_tuple = tuple(command)
    if not command_tuple:
        raise ValueError("runner command cannot be empty")

    def run(fixture_bytes: bytes) -> bytes:
        completed = subprocess.run(
            command_tuple,
            input=fixture_bytes,
            capture_output=True,
            check=False,
        )
        if completed.returncode != 0:
            raise PromotionError(
                f"candidate runner failed with exit code {completed.returncode}: "
                f"{completed.stderr.decode('utf-8', errors='replace')}"
            )
        return completed.stdout

    return run


@dataclass(frozen=True)
class PromotionManifest:
    data: Mapping[str, Any]

    @classmethod
    def from_dict(cls, value: Mapping[str, Any]) -> PromotionManifest:
        copied = json.loads(json.dumps(value, ensure_ascii=False))
        manifest = cls(copied)
        manifest.validate()
        return manifest

    @classmethod
    def load(cls, path: str | Path) -> PromotionManifest:
        return cls.from_dict(json.loads(Path(path).read_text(encoding="utf-8")))

    def validate(self) -> None:
        missing = _REQUIRED_TOP_LEVEL - set(self.data)
        if missing:
            raise PromotionError(
                f"promotion manifest missing fields: {sorted(missing)}"
            )
        status = self.data["status"]
        if status not in _STATUSES:
            raise PromotionError(f"invalid promotion status: {status}")
        for name, required in _REQUIRED_NESTED.items():
            section = self.data.get(name)
            if not isinstance(section, Mapping):
                raise PromotionError(
                    f"promotion manifest section is not an object: {name}"
                )
            missing_section = required - set(section)
            if missing_section:
                raise PromotionError(
                    f"promotion manifest {name} missing fields: {sorted(missing_section)}"
                )
        if (
            self.data["reference"]["language"] != "python"
            or self.data["candidate"]["language"] != "cpp"
        ):
            raise PromotionError("promotion languages must be python and cpp")
        if self.data["equivalence"]["type"] not in {
            "exact",
            "numeric",
            "statistical",
            "behavioral",
        }:
            raise PromotionError("unsupported equivalence type")
        if int(self.data["dataset"]["case_count"]) < 1:
            raise PromotionError("dataset case_count must be positive")

    def save(self, path: str | Path) -> None:
        target = Path(path)
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(
            json.dumps(self.data, ensure_ascii=False, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
        )

    def freeze_reference(
        self, fixture_bytes: bytes, *, commit: str
    ) -> PromotionManifest:
        if self.data["status"] not in {"draft", "reference_frozen"}:
            raise PromotionError("only a draft can freeze a reference")
        updated = _copy_data(self.data)
        updated["status"] = "reference_frozen"
        updated["reference"]["commit"] = commit
        updated["dataset"]["hash"] = _sha256(fixture_bytes)
        updated["dataset"]["case_count"] = _count_json_lines(fixture_bytes)
        return PromotionManifest.from_dict(updated)

    def change_tolerance(
        self,
        *,
        absolute: float | None = None,
        relative: float | None = None,
        justification: str = "",
        review_id: str = "",
    ) -> PromotionManifest:
        if not justification.strip() or not review_id.strip():
            raise ValueError(
                "tolerance changes require a justification and a new review"
            )
        if (
            absolute is not None
            and absolute < 0
            or relative is not None
            and relative < 0
        ):
            raise ValueError("tolerances cannot be negative")
        updated = _copy_data(self.data)
        equivalence = updated["equivalence"]
        equivalence["absolute_tolerance"] = absolute
        equivalence["relative_tolerance"] = relative
        equivalence["tolerance_revision"] = (
            int(equivalence.get("tolerance_revision", 0)) + 1
        )
        equivalence["tolerance_justification"] = justification
        equivalence["tolerance_review_id"] = review_id
        return PromotionManifest.from_dict(updated)

    def with_status(
        self, status: str, *, review_id: str | None = None
    ) -> PromotionManifest:
        if status not in _STATUSES:
            raise ValueError(f"invalid promotion status: {status}")
        if status == "promoted" and not review_id:
            raise ValueError("promoted status requires a review_id")
        updated = _copy_data(self.data)
        updated["status"] = status
        if review_id is not None:
            updated["approval_review_id"] = review_id
        return PromotionManifest.from_dict(updated)


@dataclass(frozen=True)
class PromotionResult:
    manifest: PromotionManifest
    equivalence_passed: bool
    divergences: list[dict[str, Any]]
    input_sha256: Mapping[str, str]
    output_sha256: Mapping[str, str]
    performance: Mapping[str, Any]

    @property
    def performance_passed(self) -> bool:
        return bool(self.performance.get("passed", False))

    def to_dict(self) -> dict[str, Any]:
        data = self.manifest.data
        return {
            "promotion_id": data["promotion_id"],
            "component_id": data["component_id"],
            "hypothesis": data["hypothesis"],
            "commits": {
                "python": data["reference"]["commit"],
                "cpp": data["candidate"]["commit"],
            },
            "hardware": {
                "system": platform.system(),
                "release": platform.release(),
                "machine": platform.machine(),
                "processor": platform.processor() or "unknown",
            },
            "dataset": data["dataset"],
            "equivalence": {
                "passed": self.equivalence_passed,
                "input_sha256": dict(self.input_sha256),
                "output_sha256": dict(self.output_sha256),
                "divergences": list(self.divergences),
            },
            "performance": dict(self.performance),
            "manifest": data,
        }

    def write_report(self, path: str | Path) -> None:
        target = Path(path)
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(
            json.dumps(self.to_dict(), ensure_ascii=False, indent=2, sort_keys=True)
            + "\n",
            encoding="utf-8",
        )


class PromotionPipeline:
    def evaluate(
        self,
        manifest: PromotionManifest,
        fixture_bytes: bytes,
        *,
        reference_runner: Runner,
        candidate_runner: Runner,
        divergence_path: str | Path | None = None,
        performance_metrics: Mapping[str, float] | None = None,
    ) -> PromotionResult:
        manifest.validate()
        if manifest.data["status"] not in {
            "reference_frozen",
            "candidate_ready",
            "validated",
            "promoted",
        }:
            raise PromotionError("promotion reference must be frozen before evaluation")
        expected_hash = manifest.data["dataset"]["hash"]
        if expected_hash and expected_hash != _sha256(fixture_bytes):
            raise PromotionError("fixture bytes do not match frozen reference dataset")
        python_output, python_failures = _execute_runner(
            "python", reference_runner, fixture_bytes
        )
        cpp_output, cpp_failures = _execute_runner(
            "cpp", candidate_runner, fixture_bytes
        )
        divergences = python_failures + cpp_failures
        if not python_failures and not cpp_failures:
            divergences.extend(
                _compare_outputs(
                    python_output,
                    cpp_output,
                    manifest.data["equivalence"],
                )
            )
        if divergence_path is not None:
            target = Path(divergence_path)
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_text(
                json.dumps(divergences, ensure_ascii=False, indent=2, sort_keys=True)
                + "\n",
                encoding="utf-8",
            )
        performance = _performance_gate(
            manifest.data["performance"], performance_metrics
        )
        return PromotionResult(
            manifest=manifest,
            equivalence_passed=not divergences,
            divergences=divergences,
            input_sha256={
                "python": _sha256(fixture_bytes),
                "cpp": _sha256(fixture_bytes),
            },
            output_sha256={
                "python": _sha256(python_output),
                "cpp": _sha256(cpp_output),
            },
            performance=performance,
        )


def compare_outputs(
    reference: bytes, candidate: bytes, equivalence: Mapping[str, Any]
) -> list[dict[str, Any]]:
    """Return every semantic divergence between two runner outputs."""

    return _compare_outputs(reference, candidate, equivalence)


def evaluate_performance(
    limits: Mapping[str, Any], metrics: Mapping[str, float] | None
) -> dict[str, Any]:
    """Evaluate the declared performance limits without mixing metric classes."""

    return _performance_gate(limits, metrics)


class PromotionRegistry:
    """Persistent approved-promotion registry used by CI and local tools."""

    def __init__(self, path: str | Path) -> None:
        self.path = Path(path)
        if self.path.exists():
            self._data = json.loads(self.path.read_text(encoding="utf-8"))
        else:
            self._data = {"schema_version": "1.0", "promotions": {}}
        if not isinstance(self._data.get("promotions"), dict):
            raise PromotionError("promotion registry promotions must be an object")

    def approve(self, manifest: PromotionManifest) -> None:
        manifest.validate()
        if manifest.data["status"] != "promoted" or not manifest.data.get(
            "approval_review_id"
        ):
            raise PromotionGateError(
                "only reviewed promoted manifests can enter the registry"
            )
        self._data["promotions"][manifest.data["component_id"]] = dict(manifest.data)
        self._save()

    def require(self, component_id: str) -> dict[str, Any]:
        record = self._data["promotions"].get(component_id)
        if not record or record.get("status") != "promoted":
            raise PromotionGateError(
                f"component has no approved promotion: {component_id}"
            )
        return record

    def _save(self) -> None:
        self.path.parent.mkdir(parents=True, exist_ok=True)
        self.path.write_text(
            json.dumps(self._data, ensure_ascii=False, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
        )


def check_required_components(
    required_path: str | Path, registry_path: str | Path
) -> list[str]:
    required = json.loads(Path(required_path).read_text(encoding="utf-8"))
    components = required.get("components", [])
    registry = PromotionRegistry(registry_path)
    missing: list[str] = []
    for component in components:
        try:
            registry.require(str(component))
        except PromotionGateError:
            missing.append(str(component))
    return missing


def _compare_outputs(
    reference: bytes, candidate: bytes, equivalence: Mapping[str, Any]
) -> list[dict[str, Any]]:
    reference_lines = _json_lines(reference)
    candidate_lines = _json_lines(candidate)
    divergences: list[dict[str, Any]] = []
    for index in range(max(len(reference_lines), len(candidate_lines))):
        if index >= len(reference_lines) or index >= len(candidate_lines):
            divergences.append(
                {"case_index": index, "classification": "case-count-mismatch"}
            )
            continue
        equal = _semantic_equal(
            reference_lines[index], candidate_lines[index], equivalence
        )
        if not equal:
            divergences.append(
                {
                    "case_index": index,
                    "classification": "semantic-mismatch",
                    "reference": reference_lines[index],
                    "candidate": candidate_lines[index],
                }
            )
    return divergences


def _execute_runner(
    label: str, runner: Runner, fixture_bytes: bytes
) -> tuple[bytes, list[dict[str, Any]]]:
    try:
        output = runner(fixture_bytes)
    except Exception as exc:  # noqa: BLE001 - runner failures are promotion evidence.
        return b"", [
            {
                "case_index": None,
                "classification": "runner-failure",
                "runner": label,
                "error": f"{type(exc).__name__}: {exc}",
            }
        ]
    if not isinstance(output, bytes):
        return b"", [
            {
                "case_index": None,
                "classification": "runner-invalid-output",
                "runner": label,
                "error": "runner must return bytes",
            }
        ]
    try:
        _json_lines(output)
    except PromotionError as exc:
        return b"", [
            {
                "case_index": None,
                "classification": "runner-invalid-output",
                "runner": label,
                "error": str(exc),
            }
        ]
    return output, []


def _json_lines(value: bytes) -> list[Any]:
    lines = [line for line in value.splitlines() if line.strip()]
    try:
        return [json.loads(line.decode("utf-8")) for line in lines]
    except (UnicodeDecodeError, json.JSONDecodeError) as exc:
        raise PromotionError(
            f"runner output is not canonical JSON lines: {exc}"
        ) from exc


def _semantic_equal(first: Any, second: Any, equivalence: Mapping[str, Any]) -> bool:
    if equivalence["type"] != "numeric":
        return canonical_json_bytes(first) == canonical_json_bytes(second)
    absolute = float(equivalence.get("absolute_tolerance") or 0.0)
    relative = float(equivalence.get("relative_tolerance") or 0.0)
    return _numeric_equal(first, second, absolute, relative)


def _numeric_equal(first: Any, second: Any, absolute: float, relative: float) -> bool:
    if isinstance(first, bool) or isinstance(second, bool):
        return first == second
    if isinstance(first, (int, float)) and isinstance(second, (int, float)):
        return math.isclose(
            float(first), float(second), abs_tol=absolute, rel_tol=relative
        )
    if isinstance(first, Mapping) and isinstance(second, Mapping):
        return set(first) == set(second) and all(
            _numeric_equal(first[key], second[key], absolute, relative) for key in first
        )
    if isinstance(first, list) and isinstance(second, list):
        return len(first) == len(second) and all(
            _numeric_equal(left, right, absolute, relative)
            for left, right in zip(first, second, strict=True)
        )
    return first == second


def _performance_gate(
    limits: Mapping[str, Any], metrics: Mapping[str, float] | None
) -> dict[str, Any]:
    if metrics is None:
        return {"passed": False, "missing": ["metrics"]}
    checks = {
        "latency_ms": ("maximum_latency_ms", lambda actual, limit: actual <= limit),
        "memory_mb": ("maximum_memory_mb", lambda actual, limit: actual <= limit),
        "throughput": ("minimum_throughput", lambda actual, limit: actual >= limit),
    }
    failures: list[str] = []
    for metric, (limit_name, predicate) in checks.items():
        limit = limits.get(limit_name)
        if limit is not None and (
            metric not in metrics or not predicate(float(metrics[metric]), float(limit))
        ):
            failures.append(metric)
    return {"passed": not failures, "metrics": dict(metrics), "failures": failures}


def _copy_data(value: Mapping[str, Any]) -> dict[str, Any]:
    return json.loads(json.dumps(value, ensure_ascii=False))


def _sha256(value: bytes) -> str:
    return hashlib.sha256(value).hexdigest()


def _count_json_lines(value: bytes) -> int:
    count = len([line for line in value.splitlines() if line.strip()])
    if count < 1:
        raise PromotionError("fixture set cannot be empty")
    return count
