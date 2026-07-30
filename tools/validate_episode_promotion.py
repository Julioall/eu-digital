"""Run SPEC-033 equivalence, holdout, invariant, ablation and operational gates."""

from __future__ import annotations

import argparse
import hashlib
import json
import resource
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

from eu_digital_lab.episode_segmentation import SegmentConfig, boundary_metrics, segment_events
from eu_digital_lab.promotion import (
    PromotionManifest,
    PromotionPipeline,
    compare_outputs,
    python_runner,
)


def json_lines(path: Path) -> list[dict[str, Any]]:
    return [json.loads(line) for line in path.read_text(encoding="utf-8").splitlines() if line.strip()]


def canonical_reference(case: dict[str, Any]) -> dict[str, Any]:
    return segment_events(case["events"], SegmentConfig(**case["config"])).to_mapping()


def native_runner(binary: Path) -> Any:
    def run(fixture: bytes) -> bytes:
        completed = subprocess.run(
            [str(binary), "--episode-segmentation"],
            input=fixture,
            capture_output=True,
            check=False,
        )
        if completed.returncode:
            raise RuntimeError(completed.stderr.decode("utf-8", errors="replace"))
        return completed.stdout

    return run


def fixture_bytes(path: Path) -> bytes:
    return path.read_bytes()


def invariant_failures(case: dict[str, Any], result: dict[str, Any]) -> list[str]:
    event_ids = [event["event_id"] for event in case["events"]]
    episodes = result.get("episodes", [])
    failures: list[str] = []
    covered = [event_id for episode in episodes for event_id in episode.get("event_ids", [])]
    if covered != event_ids or len(set(covered)) != len(covered):
        failures.append("all_event_ids_are_covered_once")
    if any(not episode.get("event_ids") for episode in episodes):
        failures.append("episodes_are_non_empty_and_contiguous")
    boundaries = result.get("boundaries", [])
    if not boundaries or boundaries[0].get("event_id") != event_ids[0] or boundaries[0].get("reasons") != ["episode_start"]:
        failures.append("first_boundary_is_episode_start")
    if any(not boundary.get("reasons") for boundary in boundaries):
        failures.append("every_boundary_has_reason")
    return failures


def aggregate_metrics(cases: list[dict[str, Any]], outputs: list[dict[str, Any]]) -> dict[str, float]:
    values = [
        boundary_metrics(output["episodes"], case["ground_truth"], [event["event_id"] for event in case["events"]])
        for case, output in zip(cases, outputs, strict=True)
    ]
    return {
        key: statistics.fmean(float(item[key]) for item in values)
        for key in ("boundary_precision", "boundary_recall", "boundary_f1", "window_diff")
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--candidate", type=Path, default=ROOT / "build" / "dev" / "promotion_fixture_runner")
    parser.add_argument("--fixture", type=Path, default=ROOT / "validation" / "equivalence" / "episode_segmentation_v1.jsonl")
    parser.add_argument("--holdout", type=Path, default=ROOT / "validation" / "holdout" / "episode_segmentation_v1_holdout.jsonl")
    parser.add_argument("--manifest", type=Path, default=ROOT / "promotions" / "cognition.episode_segmentation.v1.json")
    parser.add_argument("--report", type=Path, default=ROOT / "validation" / "reports" / "episode_segmentation_v1.json")
    args = parser.parse_args()

    manifest = PromotionManifest.load(args.manifest)
    development_bytes = fixture_bytes(args.fixture)
    holdout_bytes = fixture_bytes(args.holdout)
    if hashlib.sha256(development_bytes).hexdigest() != manifest.data["dataset"]["hash"]:
        raise RuntimeError("development fixture hash does not match frozen manifest")
    if hashlib.sha256(holdout_bytes).hexdigest() != manifest.data["validation"]["holdout_hash"]:
        raise RuntimeError("holdout fixture hash does not match frozen manifest")
    if not args.candidate.exists():
        raise RuntimeError(f"candidate runner not found: {args.candidate}; build it first")

    reference = python_runner(canonical_reference)
    candidate = native_runner(args.candidate)
    timings: list[float] = []
    for _ in range(25):
        started = time.perf_counter_ns()
        candidate(development_bytes)
        timings.append((time.perf_counter_ns() - started) / 1_000_000.0)
    # Linux reports ru_maxrss in KiB. This is the maximum observed child RSS
    # for this validation process; it is intentionally reported as an
    # operational estimate, not as a cognitive metric.
    after_rusage = resource.getrusage(resource.RUSAGE_CHILDREN).ru_maxrss
    memory_mb = after_rusage / 1024.0
    sorted_timings = sorted(timings)
    p95 = sorted_timings[min(len(sorted_timings) - 1, int(len(sorted_timings) * 0.95))]
    performance_metrics = {
        "latency_ms": p95,
        "memory_mb": memory_mb,
        "throughput": len(json_lines(args.fixture)) / (statistics.fmean(timings) / 1000.0),
        "latency_p50_ms": statistics.median(timings),
        "latency_p95_ms": p95,
        "latency_max_ms": max(timings),
    }
    result = PromotionPipeline().evaluate(
        manifest,
        development_bytes,
        reference_runner=reference,
        candidate_runner=candidate,
        divergence_path=ROOT / "validation" / "reports" / "episode_segmentation_v1_divergences.json",
        performance_metrics=performance_metrics,
    )
    if not result.equivalence_passed:
        raise RuntimeError(f"development equivalence failed: {result.divergences}")
    if not result.performance_passed:
        raise RuntimeError(f"performance gate failed: {result.performance}")

    holdout_cases = json_lines(args.holdout)
    holdout_reference = reference(holdout_bytes)
    holdout_candidate = candidate(holdout_bytes)
    holdout_divergences = compare_outputs(holdout_reference, holdout_candidate, manifest.data["equivalence"])
    holdout_outputs = [json.loads(line) for line in holdout_candidate.splitlines() if line.strip()]
    if holdout_divergences:
        raise RuntimeError(f"holdout equivalence failed: {holdout_divergences}")
    invariant_failures_by_case = {
        case["case_id"]: invariant_failures(case, output)
        for case, output in zip(holdout_cases, holdout_outputs, strict=True)
    }
    development_outputs = [json.loads(line) for line in candidate(development_bytes).splitlines() if line.strip()]
    invariant_failures_by_case.update(
        {
            case["case_id"]: invariant_failures(case, output)
            for case, output in zip(json_lines(args.fixture), development_outputs, strict=True)
        }
    )
    invariant_failures_flat = {
        case_id: failures for case_id, failures in invariant_failures_by_case.items() if failures
    }
    if invariant_failures_flat:
        raise RuntimeError(f"invariant gate failed: {invariant_failures_flat}")

    development_cases = json_lines(args.fixture)
    holdout_metrics = aggregate_metrics(holdout_cases, holdout_outputs)
    baseline_outputs = [canonical_reference({**case, "config": {**case["config"], "split_on_application_change": True, "split_on_document_change": True}}) for case in development_cases]
    ablation_outputs = [canonical_reference({**case, "config": {**case["config"], "split_on_application_change": False, "split_on_document_change": False}}) for case in development_cases]
    report = result.to_dict()
    report["scientific_evidence_boundary"] = "cross-language equivalence is computational verification, not ground truth"
    report["holdout"] = {
        "fixture_set": str(args.holdout.relative_to(ROOT)),
        "sha256": hashlib.sha256(holdout_bytes).hexdigest(),
        "case_count": len(holdout_cases),
        "equivalence_passed": not holdout_divergences,
        "metrics": holdout_metrics,
    }
    report["baseline"] = aggregate_metrics(development_cases, baseline_outputs)
    report["ablation"] = aggregate_metrics(development_cases, ablation_outputs)
    report["invariants"] = {"passed": True, "failures": invariant_failures_flat}
    report["operational_measurement"] = {
        "metrics": performance_metrics,
        "interpretation": "operational only; no cognitive claim",
    }
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(report, ensure_ascii=False, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(json.dumps({"promotion_id": manifest.data["promotion_id"], "equivalence": True, "holdout": True, "performance": result.performance}, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
