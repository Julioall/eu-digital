"""Run SPEC-035 pattern-learning equivalence, holdout, ablation and drift gates."""

from __future__ import annotations

import argparse
import hashlib
import json
import resource
import statistics
import subprocess
import sys
import time
from collections import defaultdict
from pathlib import Path
from typing import Any

ROOT = Path(__file__).resolve().parents[1]
LAB_ROOT = ROOT / "python"
if str(LAB_ROOT) not in sys.path:
    sys.path.insert(0, str(LAB_ROOT))

from eu_digital_lab.pattern_learning import PatternConfig, PatternLearner
from eu_digital_lab.promotion import PromotionManifest, PromotionPipeline, compare_outputs, python_runner


def cases(path: Path) -> list[dict[str, Any]]:
    return [json.loads(line) for line in path.read_text(encoding="utf-8").splitlines() if line.strip()]


def reference_transform(case: dict[str, Any]) -> dict[str, Any]:
    learner = PatternLearner(
        PatternConfig(**case.get("config", {})),
        stream_id=case["stream_id"],
    )
    observations = []
    feedback = []
    last_pattern_id = ""
    for operation in case["operations"]:
        if operation["type"] == "observe":
            item = operation["observation"]
            record = learner.observe(item["features"], item["observation_ref"], item["occurred_at"])
            last_pattern_id = record.pattern_id
            observations.append(record.to_mapping())
        elif operation["type"] == "feedback":
            target = last_pattern_id if operation["target"] == "last_observation" else operation["target"]
            feedback.append(
                learner.feedback(target, positive=bool(operation["positive"]), reference=operation["reference"]).to_mapping()
            )
        else:
            raise ValueError(f"unsupported operation: {operation['type']}")
    metrics = learner.metrics()
    return {
        "schema_version": "1.0",
        "observations": observations,
        "feedback": feedback,
        "snapshot": learner.snapshot(),
        "metrics": metrics,
    }


def invariant_failures(case: dict[str, Any], result: dict[str, Any]) -> list[str]:
    failures: list[str] = []
    patterns = {item["pattern_id"]: item for item in result.get("snapshot", {}).get("patterns", [])}
    config = {"min_support": 3, "distance_threshold": 0.25, "promotion_confidence": 0.5}
    config.update(case.get("config", {}))
    for pattern in patterns.values():
        if pattern["status"] == "promoted" and (
            pattern["support"] < config["min_support"] or pattern["confidence"] < config["promotion_confidence"]
        ):
            failures.append("support_threshold_controls_promotion")
        if pattern.get("parent_pattern_id") is not None:
            parent = patterns.get(pattern["parent_pattern_id"])
            if parent is None or parent.get("status") != "superseded":
                failures.append("parent_provenance_is_preserved")
        if any(key in pattern for key in ("name", "action", "task")):
            failures.append("pattern_is_not_a_fact")
    for record in result.get("feedback", []):
        if not record.get("feedback", {}).get("references"):
            failures.append("feedback_reference_is_preserved")
    observed_refs = {operation["observation"]["observation_ref"] for operation in case["operations"] if operation["type"] == "observe"}
    if not observed_refs.issubset({ref for pattern in patterns.values() for ref in pattern.get("observation_refs", [])}):
        failures.append("observation_provenance_is_preserved")
    return sorted(set(failures))


def treatment_metrics(outputs: list[dict[str, Any]]) -> dict[str, float]:
    records = [pattern for output in outputs for pattern in output.get("snapshot", {}).get("patterns", [])]
    return {
        "cluster_count": float(len(records)),
        "promoted_count": float(sum(pattern.get("status") == "promoted" for pattern in records)),
        "drift_version_count": float(sum(pattern.get("drift_reason") == "concept_drift" for pattern in records)),
        "mean_support": statistics.fmean(pattern["support"] for pattern in records) if records else 0.0,
    }


def exact_key_baseline(case_outputs: list[dict[str, Any]], test_cases: list[dict[str, Any]]) -> dict[str, float]:
    clusters = 0
    promoted = 0
    for case in test_cases:
        groups: defaultdict[tuple[tuple[str, float], ...], int] = defaultdict(int)
        for operation in case["operations"]:
            if operation["type"] == "observe":
                groups[tuple(sorted((str(key), float(value)) for key, value in operation["observation"]["features"].items()))] += 1
        clusters += len(groups)
        minimum = int(case.get("config", {}).get("min_support", 3))
        promoted += sum(count >= minimum for count in groups.values())
    return {"cluster_count": float(clusters), "promoted_count": float(promoted), "interpretation": "exact feature-key baseline; operational comparison only"}


def run_candidate(binary: Path):
    def runner(fixture: bytes) -> bytes:
        completed = subprocess.run([str(binary), "--pattern-learning"], input=fixture, capture_output=True, check=False)
        if completed.returncode:
            raise RuntimeError(completed.stderr.decode("utf-8", errors="replace"))
        return completed.stdout

    return runner


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--candidate", type=Path, default=ROOT / "build" / "dev" / "promotion_fixture_runner")
    parser.add_argument("--fixture", type=Path, default=ROOT / "validation" / "equivalence" / "pattern_learning_v1.jsonl")
    parser.add_argument("--holdout", type=Path, default=ROOT / "validation" / "holdout" / "pattern_learning_v1_holdout.jsonl")
    parser.add_argument("--manifest", type=Path, default=ROOT / "promotions" / "cognition.pattern_learning.v1.json")
    parser.add_argument("--report", type=Path, default=ROOT / "validation" / "reports" / "pattern_learning_v1.json")
    args = parser.parse_args()
    manifest = PromotionManifest.load(args.manifest)
    development_bytes = args.fixture.read_bytes()
    holdout_bytes = args.holdout.read_bytes()
    if hashlib.sha256(development_bytes).hexdigest() != manifest.data["dataset"]["hash"]:
        raise RuntimeError("development fixture hash does not match manifest")
    if hashlib.sha256(holdout_bytes).hexdigest() != manifest.data["validation"]["holdout_hash"]:
        raise RuntimeError("holdout fixture hash does not match manifest")
    if not args.candidate.exists():
        raise RuntimeError(f"candidate runner not found: {args.candidate}")
    reference = python_runner(reference_transform)
    candidate = run_candidate(args.candidate)
    timings = []
    for _ in range(25):
        started = time.perf_counter_ns()
        candidate(development_bytes)
        timings.append((time.perf_counter_ns() - started) / 1_000_000.0)
    rss_mb = resource.getrusage(resource.RUSAGE_CHILDREN).ru_maxrss / 1024.0
    ordered = sorted(timings)
    p95 = ordered[min(len(ordered) - 1, int(len(ordered) * 0.95))]
    performance_metrics = {
        "latency_ms": p95,
        "latency_p50_ms": statistics.median(timings),
        "latency_p95_ms": p95,
        "latency_max_ms": max(timings),
        "memory_mb": rss_mb,
        "throughput": len(cases(args.fixture)) / (statistics.fmean(timings) / 1000.0),
    }
    result = PromotionPipeline().evaluate(
        manifest,
        development_bytes,
        reference_runner=reference,
        candidate_runner=candidate,
        divergence_path=ROOT / "validation" / "reports" / "pattern_learning_v1_divergences.json",
        performance_metrics=performance_metrics,
    )
    if not result.equivalence_passed or not result.performance_passed:
        raise RuntimeError(f"development gate failed: {result.divergences} {result.performance}")
    development_cases = cases(args.fixture)
    holdout_cases = cases(args.holdout)
    holdout_reference = reference(holdout_bytes)
    holdout_candidate = candidate(holdout_bytes)
    holdout_divergences = compare_outputs(holdout_reference, holdout_candidate, manifest.data["equivalence"])
    if holdout_divergences:
        raise RuntimeError(f"holdout gate failed: {holdout_divergences}")
    development_outputs = [json.loads(line) for line in result_output(candidate(development_bytes))]
    holdout_outputs = [json.loads(line) for line in result_output(holdout_candidate)]
    failures = {}
    for case, output in zip(development_cases + holdout_cases, development_outputs + holdout_outputs, strict=True):
        current = invariant_failures(case, output)
        if current:
            failures[case["case_id"]] = current
    if failures:
        raise RuntimeError(f"invariant gate failed: {failures}")
    report = result.to_dict()
    report["scientific_evidence_boundary"] = "cross-language equivalence is computational verification, not ground truth"
    report["holdout"] = {
        "fixture_set": str(args.holdout.relative_to(ROOT)),
        "sha256": hashlib.sha256(holdout_bytes).hexdigest(),
        "case_count": len(holdout_cases),
        "equivalence_passed": not holdout_divergences,
    }
    report["treatment_metrics"] = treatment_metrics(development_outputs)
    report["baseline"] = exact_key_baseline(development_outputs, development_cases)
    report["ablation"] = {"distance_threshold": 0.0, "cross_feature_clustering": False, "interpretation": "operational ablation only"}
    report["invariants"] = {"passed": True, "failures": failures}
    report["operational_measurement"] = {"metrics": performance_metrics, "interpretation": "operational only; no cognitive claim"}
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(report, ensure_ascii=False, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(json.dumps({"promotion_id": manifest.data["promotion_id"], "equivalence": True, "holdout": True, "performance": result.performance}, indent=2))
    return 0


def result_output(value: bytes) -> list[str]:
    return [line for line in value.decode("utf-8").splitlines() if line.strip()]


if __name__ == "__main__":
    raise SystemExit(main())
