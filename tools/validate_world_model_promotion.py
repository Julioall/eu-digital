"""Run SPEC-036 world-model equivalence, holdout and scientific gates."""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import os
import platform
import statistics
import subprocess
import sys
import time
from pathlib import Path
from typing import Any

ROOT = Path(__file__).resolve().parents[1]
LAB_ROOT = ROOT / "python"
if str(LAB_ROOT) not in sys.path:
    sys.path.insert(0, str(LAB_ROOT))

from eu_digital_lab.promotion import (
    PromotionManifest,
    PromotionPipeline,
    compare_outputs,
    python_runner,
)
from eu_digital_lab.world_model import (
    ModelPolicy,
    PredictionConfig,
    WorldModel,
)


def cases(path: Path) -> list[dict[str, Any]]:
    return [
        json.loads(line)
        for line in path.read_text(encoding="utf-8").splitlines()
        if line.strip()
    ]


def canonical_sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes().replace(b"\r\n", b"\n")).hexdigest()


def reference_transform(case: dict[str, Any]) -> dict[str, Any]:
    model = WorldModel(
        stream_id=case["stream_id"],
        policy=case.get("policy", ModelPolicy.incremental),
        config=PredictionConfig(**case.get("config", {})),
        promoted_patterns=case.get("patterns", []),
    )
    predictions: list[Any] = []
    errors: list[dict[str, Any]] = []
    last_prediction_id = ""
    drift_ids: set[str] = set()
    drifts: list[dict[str, Any]] = []
    for operation in case["operations"]:
        operation_type = operation["type"]
        if operation_type == "observe":
            item = operation["observation"]
            model.observe(item["state"], item["event_ref"], item["occurred_at"])
        elif operation_type == "predict":
            prediction = model.predict(
                operation.get("context", []),
                predicted_at=operation["predicted_at"],
                candidate_states=operation.get("candidate_states", []),
            )
            predictions.append(prediction)
            last_prediction_id = prediction.prediction_id
        elif operation_type == "score":
            target = operation["target"]
            target_id = last_prediction_id if target == "last_prediction" else target
            index = next(
                index
                for index, prediction in enumerate(predictions)
                if prediction.prediction_id == target_id
            )
            scored = model.score(
                predictions[index],
                operation["observed_state"],
                operation["observed_at"],
            )
            predictions[index] = scored
            errors.append(scored.error_mapping(operation["observed_at"]))
            latest = model.latest_drift()
            if latest is not None and latest.drift_id not in drift_ids:
                drift_ids.add(latest.drift_id)
                drifts.append(latest.to_mapping())
        else:
            raise ValueError(f"unsupported world-model operation: {operation_type}")
    return {
        "drifts": drifts,
        "errors": errors,
        "metrics": model.metrics(),
        "predictions": [prediction.to_mapping() for prediction in predictions],
        "schema_version": "1.0",
    }


def invariant_failures(case: dict[str, Any], result: dict[str, Any]) -> list[str]:
    failures: list[str] = []
    predictions = result.get("predictions", [])
    errors = result.get("errors", [])
    for prediction in predictions:
        distribution = prediction.get("predicted_distribution", {})
        total = sum(float(value) for value in distribution.values())
        if not math.isclose(total, 1.0, abs_tol=1e-12):
            failures.append("distribution_is_normalized")
        if (
            prediction.get("observed_state") is None
            and prediction.get("log_loss") is not None
        ):
            failures.append("prediction_is_uncertain_until_observed")
        if (
            prediction.get("observed_state") is None
            and prediction.get("top_k_hit") is not None
        ):
            failures.append("prediction_is_uncertain_until_observed")
        if any(
            key in prediction for key in ("fact", "action", "task", "inferred_state")
        ):
            failures.append("world_model_does_not_create_facts_or_actions")
    prediction_ids = {prediction.get("prediction_id") for prediction in predictions}
    if any(error.get("prediction_id") not in prediction_ids for error in errors):
        failures.append("prediction_error_provenance_is_explicit")
    if any(error.get("observed_state") is None for error in errors):
        failures.append("prediction_error_requires_observation")
    ground_truth = case.get("ground_truth", {})
    if case.get("patterns"):
        expected_patterns = len(case["patterns"])
        actual_patterns = int(
            result.get("metrics", {}).get("promoted_pattern_count", -1)
        )
        if actual_patterns != expected_patterns:
            failures.append("promoted_pattern_input_is_explicit")
    if ground_truth.get("uniform_without_observation"):
        distribution = (
            predictions[0].get("predicted_distribution", {}) if predictions else {}
        )
        if (
            not distribution
            or len({round(float(value), 12) for value in distribution.values()}) != 1
        ):
            failures.append("absence_is_not_negative_observation")
    expected_drift_count = ground_truth.get("drift_count")
    if expected_drift_count is not None and len(result.get("drifts", [])) != int(
        expected_drift_count
    ):
        failures.append("drift_is_explicit")
    if ground_truth.get("confidence_reduced"):
        drift = result.get("drifts", [None])[0]
        if not drift or not float(drift["confidence_after"]) < float(
            drift["confidence_before"]
        ):
            failures.append("confidence_is_bounded_and_reduced_on_drift")
    return sorted(set(failures))


def scored_metrics(outputs: list[dict[str, Any]]) -> dict[str, float | None]:
    errors = [error for output in outputs for error in output.get("errors", [])]
    if not errors:
        return {"mean_log_loss": None, "top_k_accuracy": None, "error_count": 0.0}
    return {
        "mean_log_loss": statistics.fmean(float(error["log_loss"]) for error in errors),
        "top_k_accuracy": statistics.fmean(
            bool(error["top_k_hit"]) for error in errors
        ),
        "error_count": float(len(errors)),
    }


def calibration_metrics(outputs: list[dict[str, Any]]) -> dict[str, float | None]:
    pairs: list[tuple[float, bool, float]] = []
    for output in outputs:
        predictions = {
            item["prediction_id"]: item for item in output.get("predictions", [])
        }
        for error in output.get("errors", []):
            prediction = predictions[error["prediction_id"]]
            distribution = prediction["predicted_distribution"]
            observed = error["observed_state"]
            probability = float(distribution.get(observed, 0.0))
            confidence = max(float(value) for value in distribution.values())
            pairs.append((confidence, bool(error["top_k_hit"]), probability))
    if not pairs:
        return {"brier_score": None, "expected_calibration_error": None}
    brier = statistics.fmean(
        sum(
            (float(probability) - (1.0 if state == observed else 0.0)) ** 2
            for state, probability in distribution.items()
        )
        for output in outputs
        for error in output.get("errors", [])
        for distribution in [
            {
                state: float(value)
                for state, value in next(
                    item["predicted_distribution"]
                    for item in output.get("predictions", [])
                    if item["prediction_id"] == error["prediction_id"]
                ).items()
            }
        ]
        for observed in [error["observed_state"]]
    )
    bins: dict[int, list[tuple[float, bool]]] = {}
    for confidence, hit, probability in pairs:
        del probability
        bucket = min(9, int(confidence * 10))
        bins.setdefault(bucket, []).append((confidence, hit))
    ece = sum(
        len(values)
        / len(pairs)
        * abs(
            statistics.fmean(confidence for confidence, unused in values)
            - statistics.fmean(hit for unused, hit in values)
        )
        for values in bins.values()
    )
    return {"brier_score": brier, "expected_calibration_error": ece}


def baseline_case(
    case: dict[str, Any], policy: str, *, clear_context: bool = False
) -> dict[str, Any]:
    copied = json.loads(json.dumps(case))
    copied["policy"] = policy
    if clear_context:
        for operation in copied["operations"]:
            if operation["type"] == "predict":
                operation["context"] = []
    return copied


def current_process_memory_mb() -> float:
    try:
        import resource

        return resource.getrusage(resource.RUSAGE_CHILDREN).ru_maxrss / 1024.0
    except (ImportError, AttributeError):
        if platform.system() != "Windows":
            return 0.0
        import ctypes
        from ctypes import wintypes

        class ProcessMemoryCounters(ctypes.Structure):
            _fields_ = [
                ("cb", wintypes.DWORD),
                ("PageFaultCount", wintypes.DWORD),
                ("PeakWorkingSetSize", ctypes.c_size_t),
                ("WorkingSetSize", ctypes.c_size_t),
                ("QuotaPeakPagedPoolUsage", ctypes.c_size_t),
                ("QuotaPagedPoolUsage", ctypes.c_size_t),
                ("QuotaPeakNonPagedPoolUsage", ctypes.c_size_t),
                ("QuotaNonPagedPoolUsage", ctypes.c_size_t),
                ("PagefileUsage", ctypes.c_size_t),
                ("PeakPagefileUsage", ctypes.c_size_t),
            ]

        counters = ProcessMemoryCounters()
        counters.cb = ctypes.sizeof(counters)
        process = ctypes.windll.kernel32.GetCurrentProcess()
        if ctypes.windll.psapi.GetProcessMemoryInfo(
            process, ctypes.byref(counters), counters.cb
        ):
            return counters.WorkingSetSize / (1024.0 * 1024.0)
        minimum = ctypes.c_size_t()
        maximum = ctypes.c_size_t()
        get_working_set = ctypes.windll.kernel32.GetProcessWorkingSetSize
        get_working_set.argtypes = [
            wintypes.HANDLE,
            ctypes.POINTER(ctypes.c_size_t),
            ctypes.POINTER(ctypes.c_size_t),
        ]
        get_working_set.restype = wintypes.BOOL
        if get_working_set(process, ctypes.byref(minimum), ctypes.byref(maximum)):
            return minimum.value / (1024.0 * 1024.0)
        return 0.0


def run_candidate(binary: Path):
    def runner(fixture: bytes) -> bytes:
        candidate = binary
        if not candidate.exists() and os.name == "nt" and candidate.suffix == "":
            candidate = candidate.with_suffix(".exe")
        completed = subprocess.run(
            [str(candidate), "--world-model"],
            input=fixture,
            capture_output=True,
            check=False,
        )
        if completed.returncode:
            raise RuntimeError(completed.stderr.decode("utf-8", errors="replace"))
        return completed.stdout

    return runner


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--candidate",
        type=Path,
        default=ROOT / "build" / "dev" / "promotion_fixture_runner",
    )
    parser.add_argument(
        "--fixture",
        type=Path,
        default=ROOT / "validation" / "equivalence" / "world_model_prediction_v1.jsonl",
    )
    parser.add_argument(
        "--holdout",
        type=Path,
        default=ROOT
        / "validation"
        / "holdout"
        / "world_model_prediction_v1_holdout.jsonl",
    )
    parser.add_argument(
        "--manifest",
        type=Path,
        default=ROOT / "promotions" / "cognition.world_model.v1.json",
    )
    parser.add_argument(
        "--report",
        type=Path,
        default=ROOT / "validation" / "reports" / "world_model_prediction_v1.json",
    )
    args = parser.parse_args()
    manifest = PromotionManifest.load(args.manifest)
    development_bytes = args.fixture.read_bytes()
    holdout_bytes = args.holdout.read_bytes()
    if canonical_sha256(args.fixture) != manifest.data["dataset"]["hash"]:
        raise RuntimeError("development fixture hash does not match manifest")
    if canonical_sha256(args.holdout) != manifest.data["validation"]["holdout_hash"]:
        raise RuntimeError("holdout fixture hash does not match manifest")
    development_cases = cases(args.fixture)
    holdout_cases = cases(args.holdout)
    development_ids = {case["case_id"] for case in development_cases}
    holdout_ids = {case["case_id"] for case in holdout_cases}
    if development_ids & holdout_ids:
        raise RuntimeError("development and holdout cases overlap")
    reference = python_runner(reference_transform)
    candidate = run_candidate(args.candidate)
    timings: list[float] = []
    for _ in range(25):
        started = time.perf_counter_ns()
        candidate(development_bytes)
        timings.append((time.perf_counter_ns() - started) / 1_000_000.0)
    sorted_timings = sorted(timings)
    p95 = sorted_timings[min(len(sorted_timings) - 1, int(len(sorted_timings) * 0.95))]
    performance_metrics = {
        "latency_ms": p95,
        "latency_p50_ms": statistics.median(timings),
        "latency_p95_ms": p95,
        "latency_max_ms": max(timings),
        "memory_mb": current_process_memory_mb(),
        "throughput": len(development_cases) / (statistics.fmean(timings) / 1000.0),
    }
    result = PromotionPipeline().evaluate(
        manifest,
        development_bytes,
        reference_runner=reference,
        candidate_runner=candidate,
        divergence_path=ROOT
        / "validation"
        / "reports"
        / "world_model_prediction_v1_divergences.json",
        performance_metrics=performance_metrics,
    )
    if not result.equivalence_passed or not result.performance_passed:
        raise RuntimeError(
            f"development gate failed: equivalence={result.equivalence_passed}, performance={result.performance}"
        )
    holdout_reference = reference(holdout_bytes)
    holdout_candidate = candidate(holdout_bytes)
    holdout_divergences = compare_outputs(
        holdout_reference, holdout_candidate, manifest.data["equivalence"]
    )
    if holdout_divergences:
        raise RuntimeError(f"holdout gate failed: {holdout_divergences}")
    development_outputs = [
        json.loads(line) for line in result_output(candidate(development_bytes))
    ]
    holdout_outputs = [json.loads(line) for line in result_output(holdout_candidate)]
    failures = {}
    for case, output in zip(
        development_cases + holdout_cases,
        development_outputs + holdout_outputs,
        strict=True,
    ):
        current = invariant_failures(case, output)
        if current:
            failures[case["case_id"]] = current
    if failures:
        raise RuntimeError(f"invariant gate failed: {failures}")
    baseline_outputs = [
        reference_transform(baseline_case(case, "frequency_baseline_v0"))
        for case in development_cases
    ]
    ablation_outputs = [
        reference_transform(
            baseline_case(case, "incremental_markov_v1", clear_context=True)
        )
        for case in development_cases
    ]
    treatment_metrics = scored_metrics(development_outputs)
    baseline_metrics = scored_metrics(baseline_outputs)
    ablation_metrics = scored_metrics(ablation_outputs)
    if (
        treatment_metrics["mean_log_loss"] is None
        or baseline_metrics["mean_log_loss"] is None
    ):
        raise RuntimeError(
            "scientific gate requires scored treatment and baseline cases"
        )
    if float(treatment_metrics["mean_log_loss"]) >= float(
        baseline_metrics["mean_log_loss"]
    ):
        raise RuntimeError(
            "treatment does not beat the frequency baseline on development cases"
        )
    report = result.to_dict()
    report["scientific_evidence_boundary"] = (
        "cross-language equivalence and holdout are computational verification, not ground truth"
    )
    report["holdout"] = {
        "fixture_set": args.holdout.relative_to(ROOT).as_posix(),
        "sha256": canonical_sha256(args.holdout),
        "case_count": len(holdout_cases),
        "equivalence_passed": not holdout_divergences,
        "metrics": scored_metrics(holdout_outputs),
    }
    report["baseline"] = {
        "policy": "frequency_baseline_v0",
        **baseline_metrics,
        "interpretation": "operational baseline only",
    }
    report["ablation"] = {
        "policy": "incremental_markov_v1",
        "context": "cleared",
        **ablation_metrics,
        "interpretation": "operational ablation only",
    }
    report["treatment_metrics"] = {
        "policy": "incremental_markov_v1",
        **treatment_metrics,
    }
    report["calibration"] = calibration_metrics(development_outputs + holdout_outputs)
    report["invariants"] = {"passed": True, "failures": failures}
    report["operational_measurement"] = {
        "metrics": performance_metrics,
        "interpretation": "operational only; no cognitive claim",
    }
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(
        json.dumps(report, ensure_ascii=False, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    print(
        json.dumps(
            {
                "promotion_id": manifest.data["promotion_id"],
                "equivalence": True,
                "holdout": True,
                "performance": result.performance,
            },
            indent=2,
        )
    )
    return 0


def result_output(value: bytes) -> list[str]:
    return [line for line in value.decode("utf-8").splitlines() if line.strip()]


if __name__ == "__main__":
    raise SystemExit(main())
