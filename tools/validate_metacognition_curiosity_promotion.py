"""Run SPEC-039 metacognition/curiosity equivalence and scientific gates."""

from __future__ import annotations

import argparse
import hashlib
import json
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

from eu_digital_lab.metacognition_curiosity import (
    BASELINE_CONFIDENCE_ID,
    BASELINE_QUESTION_POLICY_ID,
    CuriosityConfig,
    HypothesisRecord,
    MetacognitionCuriosityEngine,
    QuestionPolicy,
    ResponseOutcome,
)
from eu_digital_lab.promotion import (
    PromotionManifest,
    PromotionPipeline,
    compare_outputs,
    python_runner,
)
from eu_digital_lab.schema_validation import validate_shared_schema


def cases(path: Path) -> list[dict[str, Any]]:
    return [
        json.loads(line)
        for line in path.read_text(encoding="utf-8").splitlines()
        if line.strip()
    ]


def canonical_sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes().replace(b"\r\n", b"\n")).hexdigest()


def config_for(
    case: dict[str, Any], override: dict[str, Any] | None = None
) -> CuriosityConfig:
    values = dict(case.get("config", {}))
    if override:
        values.update(override)
    return CuriosityConfig(**values)


def parse_hypothesis(value: dict[str, Any]) -> HypothesisRecord:
    return HypothesisRecord(
        hypothesis_id=value["hypothesis_id"],
        kind=value["kind"],
        statement=value["statement"],
        status=value["status"],
        confidence=value["confidence"],
        supporting_refs=tuple(value["evidence"]["supporting_refs"]),
        opposing_refs=tuple(value["evidence"]["opposing_refs"]),
        alternatives=tuple(value["alternatives"]),
        created_at=value["created_at"],
        updated_at=value["updated_at"],
        verification_question=value["verification"]["question"],
        expected_information_gain=value["verification"]["expected_information_gain"],
        provenance_module=value["provenance"]["module"],
        model_version=value["provenance"]["model_version"],
        schema_version=value.get("schema_version", "1.0"),
    )


def operation_id(
    operation: dict[str, Any], direct_key: str, reference_key: str, last: str
) -> str:
    if direct_key in operation:
        return operation[direct_key]
    reference = operation.get(reference_key)
    if reference == "last" or reference == "last_asked":
        if not last:
            raise ValueError(f"{reference_key} has no previous value")
        return last
    raise ValueError(f"{direct_key} or {reference_key} is required")


def reference_transform(
    case: dict[str, Any], *, config_override: dict[str, Any] | None = None
) -> dict[str, Any]:
    engine = MetacognitionCuriosityEngine(config_for(case, config_override))
    assessments: list[dict[str, Any]] = []
    questions: list[dict[str, Any]] = []
    responses: list[dict[str, Any]] = []
    snapshots: list[dict[str, Any]] = []
    metrics_reads: list[dict[str, Any]] = []
    last_assessment_id = ""
    last_question_id = ""
    last_asked_question_id = ""
    for operation in case["operations"]:
        operation_type = operation["type"]
        now = operation.get("now")
        if operation_type == "evaluate":
            assessment = engine.evaluate(parse_hypothesis(operation["hypothesis"]), now)
            assessments.append(assessment.to_mapping())
            last_assessment_id = assessment.assessment_id
        elif operation_type == "propose_question":
            question = engine.propose_question(
                operation_id(
                    operation, "assessment_id", "assessment_ref", last_assessment_id
                ),
                operation["prompt"],
                expected_resolution=operation["expected_resolution"],
                now=now,
            )
            questions.append(question.to_mapping())
            last_question_id = question.question_id
        elif operation_type == "ask":
            question = engine.ask(
                operation_id(
                    operation, "question_id", "question_ref", last_question_id
                ),
                now,
            )
            questions.append(question.to_mapping())
            last_question_id = question.question_id
            last_asked_question_id = question.question_id
        elif operation_type == "record_response":
            response = engine.record_response(
                operation_id(
                    operation,
                    "question_id",
                    "question_ref",
                    last_asked_question_id,
                ),
                outcome=ResponseOutcome(operation["outcome"]),
                correction=operation["correction"],
                evidence_refs=tuple(operation["evidence_refs"]),
                source=operation["source"],
                actor_id=operation.get("actor_id"),
                now=now,
            )
            responses.append(response.to_mapping())
        elif operation_type == "snapshot":
            snapshots.append(engine.snapshot())
        elif operation_type == "metrics":
            metrics_reads.append(engine.metrics())
        else:
            raise ValueError(f"unsupported metacognition operation: {operation_type}")
    return {
        "assessments": assessments,
        "metrics_reads": metrics_reads,
        "questions": questions,
        "responses": responses,
        "schema_version": "1.0",
        "snapshot": engine.snapshot(),
        "snapshots": snapshots,
    }


def run_candidate(binary: Path):
    def runner(fixture: bytes) -> bytes:
        candidate = binary
        if not candidate.exists() and os.name == "nt" and candidate.suffix == "":
            candidate = candidate.with_suffix(".exe")
        completed = subprocess.run(
            [str(candidate), "--metacognition-curiosity"],
            input=fixture,
            capture_output=True,
            check=False,
        )
        if completed.returncode:
            raise RuntimeError(completed.stderr.decode("utf-8", errors="replace"))
        return completed.stdout

    return runner


def result_output(value: bytes) -> list[dict[str, Any]]:
    return [
        json.loads(line) for line in value.decode("utf-8").splitlines() if line.strip()
    ]


def invariant_failures(case: dict[str, Any], result: dict[str, Any]) -> list[str]:
    failures: list[str] = []
    snapshot = result["snapshot"]
    if snapshot["schema_version"] != "1.0":
        failures.append("snapshot_schema_version")
    for hypothesis in snapshot["hypotheses"]:
        validate_shared_schema(hypothesis, "hypothesis.schema.json")
    for assessment in snapshot["assessments"]:
        validate_shared_schema(assessment, "metacognitive_assessment.schema.json")
        if assessment["decision"] not in {"question", "silence"}:
            failures.append("assessment_decision")
    assessment_ids = {item["assessment_id"] for item in snapshot["assessments"]}
    question_ids = {item["question_id"] for item in snapshot["questions"]}
    for question in snapshot["questions"]:
        validate_shared_schema(question, "curiosity_question.schema.json")
        if question["assessment_id"] not in assessment_ids:
            failures.append("question_assessment_provenance")
        if not 0.0 <= float(question["expected_information_gain"]) <= 1.0:
            failures.append("question_gain_bounded")
    for response in snapshot["responses"]:
        validate_shared_schema(response, "curiosity_response.schema.json")
        if response["question_id"] not in question_ids:
            failures.append("response_question_provenance")
    metrics = snapshot["metrics"]
    if metrics["baseline_confidence_id"] != BASELINE_CONFIDENCE_ID:
        failures.append("baseline_confidence_registered")
    if metrics["baseline_question_policy_id"] != BASELINE_QUESTION_POLICY_ID:
        failures.append("baseline_question_registered")
    calibration = metrics["calibration"]
    if not 0 <= int(calibration["outcome_count"]):
        failures.append("calibration_count")
    inconclusive_count = sum(
        response["outcome"] == "inconclusive" for response in snapshot["responses"]
    )
    verified_count = len(snapshot["responses"]) - inconclusive_count
    if int(calibration["outcome_count"]) != verified_count:
        failures.append("inconclusive_not_negative")
    if (
        case.get("ground_truth", {}).get("inconclusive_outcome_count") is not None
        and calibration["outcome_count"]
        != case["ground_truth"]["inconclusive_outcome_count"]
    ):
        failures.append("inconclusive_not_negative")
    expected_suppression = case.get("ground_truth", {}).get("suppression_reasons")
    actual_suppression = [
        question["suppression_reason"]
        for question in snapshot["questions"]
        if question["status"] == "suppressed"
    ]
    if expected_suppression is not None and actual_suppression != expected_suppression:
        failures.append("suppression_sequence")
    if case.get("ground_truth", {}).get("baseline_gain") and any(
        float(question["expected_information_gain"]) != 0.5
        for question in snapshot["questions"]
    ):
        failures.append("fixed_gain_baseline")
    forbidden = ("conscious", "phenomenal", "emotion", "feeling", "intention")
    if any(
        term in json.dumps(snapshot, ensure_ascii=False).lower() for term in forbidden
    ):
        failures.append("no_mental_state_claims")
    return sorted(set(failures))


def scientific_metrics(outputs: list[dict[str, Any]]) -> dict[str, float]:
    questions = [
        question for output in outputs for question in output["snapshot"]["questions"]
    ]
    repeated: dict[tuple[str, str], list[dict[str, Any]]] = {}
    for question in questions:
        key = (
            question["hypothesis_id"],
            " ".join(question["prompt"].casefold().split()),
        )
        repeated.setdefault(key, []).append(question)
    redundant_unsuppressed = sum(
        max(0, sum(question["status"] != "suppressed" for question in values) - 1)
        for values in repeated.values()
    )
    gains = [float(question["expected_information_gain"]) for question in questions]
    calibration = [output["snapshot"]["metrics"]["calibration"] for output in outputs]
    return {
        "question_count": float(len(questions)),
        "asked_count": float(
            sum(question["status"] in {"asked", "answered"} for question in questions)
        ),
        "suppressed_count": float(
            sum(question["status"] == "suppressed" for question in questions)
        ),
        "redundant_unsuppressed_count": float(redundant_unsuppressed),
        "expected_gain_mean": statistics.fmean(gains) if gains else 0.0,
        "calibration_outcome_count": float(
            sum(int(item["outcome_count"]) for item in calibration)
        ),
        "calibration_ece_mean": statistics.fmean(
            float(item["ece"]) for item in calibration if item["ece"] is not None
        )
        if any(item["ece"] is not None for item in calibration)
        else 0.0,
    }


def current_process_memory_mb() -> float:
    if platform.system() != "Windows":
        return 0.0
    import ctypes
    from ctypes import wintypes

    class Counters(ctypes.Structure):
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

    counters = Counters()
    counters.cb = ctypes.sizeof(counters)
    if ctypes.windll.psapi.GetProcessMemoryInfo(
        ctypes.windll.kernel32.GetCurrentProcess(),
        ctypes.byref(counters),
        counters.cb,
    ):
        return counters.WorkingSetSize / (1024.0 * 1024.0)
    return 0.0


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
        default=ROOT
        / "validation"
        / "equivalence"
        / "metacognition_curiosity_v1.jsonl",
    )
    parser.add_argument(
        "--holdout",
        type=Path,
        default=ROOT
        / "validation"
        / "holdout"
        / "metacognition_curiosity_v1_holdout.jsonl",
    )
    parser.add_argument(
        "--manifest",
        type=Path,
        default=ROOT / "promotions" / "cognition.metacognition_curiosity.v1.json",
    )
    parser.add_argument(
        "--report",
        type=Path,
        default=ROOT / "validation" / "reports" / "metacognition_curiosity_v1.json",
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
    if {case["case_id"] for case in development_cases} & {
        case["case_id"] for case in holdout_cases
    }:
        raise RuntimeError("development and holdout cases overlap")

    reference = python_runner(reference_transform)
    candidate = run_candidate(args.candidate)
    timings: list[float] = []
    for _ in range(25):
        started = time.perf_counter_ns()
        candidate(development_bytes)
        timings.append((time.perf_counter_ns() - started) / 1_000_000.0)
    p95 = statistics.quantiles(timings, n=20, method="inclusive")[18]
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
        / "metacognition_curiosity_v1_divergences.json",
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

    development_outputs = result_output(candidate(development_bytes))
    replay_outputs = result_output(candidate(development_bytes))
    if replay_outputs != development_outputs:
        raise RuntimeError("replay gate failed: candidate output is not deterministic")
    holdout_outputs = result_output(holdout_candidate)
    failures: dict[str, list[str]] = {}
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

    experimental_cases = [
        case
        for case in development_cases
        if case.get("config", {}).get("question_policy") != "fixed_gain_v0"
    ]
    experimental_outputs = [
        output
        for case, output in zip(development_cases, development_outputs, strict=True)
        if case in experimental_cases
    ]
    baseline_outputs = [
        reference_transform(
            case,
            config_override={
                "calibration_enabled": False,
                "question_policy": QuestionPolicy.fixed_gain_v0.value,
                "budget_enabled": False,
                "cooldown_enabled": False,
                "redundancy_suppression_enabled": False,
            },
        )
        for case in experimental_cases
    ]
    treatment = scientific_metrics(experimental_outputs)
    baseline = scientific_metrics(baseline_outputs)
    if (
        treatment["redundant_unsuppressed_count"]
        >= baseline["redundant_unsuppressed_count"]
    ):
        raise RuntimeError(
            "information-gain treatment does not suppress redundant proposals"
        )

    report = result.to_dict()
    report.update(
        {
            "scientific_evidence_boundary": "equivalence and holdout are computational verification; metrics are operational evidence against frozen synthetic ground truth",
            "holdout": {
                "fixture_set": args.holdout.relative_to(ROOT).as_posix(),
                "sha256": canonical_sha256(args.holdout),
                "case_count": len(holdout_cases),
                "equivalence_passed": not holdout_divergences,
                "metrics": scientific_metrics(holdout_outputs),
            },
            "baseline": {
                "confidence_policy": BASELINE_CONFIDENCE_ID,
                "question_policy": BASELINE_QUESTION_POLICY_ID,
                **baseline,
                "interpretation": "operational baseline only",
            },
            "ablation": {
                "calibration": False,
                "budget": False,
                "cooldown": False,
                "redundancy_suppression": False,
                "question_policy": BASELINE_QUESTION_POLICY_ID,
                **baseline,
                "interpretation": "operational ablation only",
            },
            "treatment_metrics": {
                "confidence_policy": "bucketed_beta_v1",
                "question_policy": "information_gain_v1",
                **treatment,
            },
            "invariants": {"passed": True, "failures": failures},
            "operational_measurement": {
                "metrics": performance_metrics,
                "interpretation": "operational only; no cognitive claim",
            },
        }
    )
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


if __name__ == "__main__":
    raise SystemExit(main())
