"""Run SPEC-033 equivalence, holdout, invariant, ablation and operational gates."""

from __future__ import annotations

import argparse
import hashlib
import json
import platform
import statistics
import subprocess
import sys
import time
from collections.abc import Mapping
from pathlib import Path
from typing import Any

resource: Any
try:
    import resource
except ModuleNotFoundError:  # pragma: no cover - exercised on Windows
    resource = None

ROOT = Path(__file__).resolve().parents[1]
LAB_ROOT = ROOT / "python"
if str(LAB_ROOT) not in sys.path:
    sys.path.insert(0, str(LAB_ROOT))

from eu_digital_lab.episode_segmentation import (
    SegmentConfig,
    boundary_metrics,
    segment_events,
)
from eu_digital_lab.promotion import (
    PromotionManifest,
    PromotionPipeline,
    compare_outputs,
    python_runner,
)
from eu_digital_lab.schema_validation import validate_shared_schema


def json_lines(path: Path) -> list[dict[str, Any]]:
    return [json.loads(line) for line in path.read_text(encoding="utf-8").splitlines() if line.strip()]


def canonical_reference(case: dict[str, Any]) -> dict[str, Any]:
    return segment_events(case["events"], SegmentConfig(**case["config"])).to_mapping()


def canonical_sha256(path: Path) -> str:
    """Hash tracked text independently of Git's platform newline conversion."""

    return hashlib.sha256(path.read_bytes().replace(b"\r\n", b"\n")).hexdigest()


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


def output_objects(value: bytes) -> list[dict[str, Any]]:
    return [json.loads(line) for line in value.splitlines() if line.strip()]


def validate_episode_outputs(outputs: list[dict[str, Any]]) -> None:
    for result in outputs:
        episodes = result.get("episodes")
        if not isinstance(episodes, list):
            raise TypeError("segmentation output must contain an episodes array")
        for episode in episodes:
            validate_shared_schema(episode, "episode.schema.json")


def process_memory_mb() -> float:
    """Return an operational memory estimate on POSIX and Windows."""

    if resource is not None:
        value = resource.getrusage(resource.RUSAGE_CHILDREN).ru_maxrss
        return value / (1024.0 * 1024.0) if platform.system() == "Darwin" else value / 1024.0
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
    get_process_memory_info = ctypes.windll.psapi.GetProcessMemoryInfo
    get_process_memory_info.argtypes = [wintypes.HANDLE, ctypes.POINTER(ProcessMemoryCounters), wintypes.DWORD]
    get_process_memory_info.restype = wintypes.BOOL
    if get_process_memory_info(process, ctypes.byref(counters), counters.cb):
        return counters.WorkingSetSize / (1024.0 * 1024.0)
    minimum = ctypes.c_size_t()
    maximum = ctypes.c_size_t()
    get_working_set = ctypes.windll.kernel32.GetProcessWorkingSetSize
    get_working_set.argtypes = [wintypes.HANDLE, ctypes.POINTER(ctypes.c_size_t), ctypes.POINTER(ctypes.c_size_t)]
    get_working_set.restype = wintypes.BOOL
    if get_working_set(process, ctypes.byref(minimum), ctypes.byref(maximum)):
        return minimum.value / (1024.0 * 1024.0)
    return 0.0


def context_value(event: Mapping[str, Any], keys: tuple[str, ...]) -> str | None:
    payload = event.get("payload", {})
    if not isinstance(payload, Mapping):
        return None
    for key in keys:
        value = payload.get(key)
        if isinstance(value, str) and value.strip():
            return value
    nested = payload.get("context", {})
    if not isinstance(nested, Mapping):
        return None
    nested_keys = ("process_name", "application", "app") if "application" in keys else ("document_uri", "document")
    for key in nested_keys:
        value = nested.get(key)
        if isinstance(value, str) and value.strip():
            return value
    return None


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
    event_by_id = {event["event_id"]: event for event in case["events"]}
    for boundary in boundaries[1:]:
        event = event_by_id.get(boundary.get("event_id"))
        if event is None:
            failures.append("boundary_references_known_event")
            continue
        if "context_change:application" in boundary.get("reasons", []) and context_value(event, ("application", "app")) is None:
            failures.append("missing_context_does_not_create_negative_evidence")
        if "context_change:document" in boundary.get("reasons", []) and context_value(event, ("document", "document_uri")) is None:
            failures.append("missing_context_does_not_create_negative_evidence")
    return failures


def metamorphic_failures(cases: list[dict[str, Any]], outputs: list[dict[str, Any]]) -> list[str]:
    by_case = {case["case_id"]: (case, output) for case, output in zip(cases, outputs, strict=True)}
    failures: list[str] = []
    missing_context = by_case.get("episode-dev-missing-context")
    if missing_context is not None and len(missing_context[1].get("episodes", [])) != 1:
        failures.append("missing_context_is_not_a_boundary_signal")
    ablation = by_case.get("episode-dev-ablation")
    if ablation is not None:
        episodes = ablation[1].get("episodes", [])
        if len(episodes) != 1 or any(
            reason.startswith("context_change:")
            for episode in episodes
            for reason in episode.get("boundary_reasons", [])
        ):
            failures.append("context_ablation_removes_context_boundaries")
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
    development_cases = json_lines(args.fixture)
    holdout_cases = json_lines(args.holdout)
    if canonical_sha256(args.fixture) != manifest.data["dataset"]["hash"]:
        raise RuntimeError("development fixture hash does not match frozen manifest")
    if canonical_sha256(args.holdout) != manifest.data["validation"]["holdout_hash"]:
        raise RuntimeError("holdout fixture hash does not match frozen manifest")
    if len(development_cases) != int(manifest.data["dataset"]["case_count"]):
        raise RuntimeError("development case count does not match frozen manifest")
    development_ids = {str(case["case_id"]) for case in development_cases}
    holdout_ids = {str(case["case_id"]) for case in holdout_cases}
    if not development_ids.isdisjoint(holdout_ids):
        raise RuntimeError("development and holdout case IDs overlap")
    reference_data = manifest.data["reference"]
    reference_path = ROOT / str(reference_data.get("source_path", "python/eu_digital_lab/episode_segmentation.py"))
    if not reference_path.exists():
        raise RuntimeError(f"frozen Python reference source not found: {reference_path}")
    reference_source_hash = canonical_sha256(reference_path)
    if reference_source_hash != reference_data.get("source_sha256"):
        raise RuntimeError("Python reference source hash does not match frozen manifest")
    if not args.candidate.exists() and sys.platform == "win32" and args.candidate.suffix == "":
        windows_candidate = args.candidate.with_suffix(".exe")
        if windows_candidate.exists():
            args.candidate = windows_candidate
    if not args.candidate.exists():
        raise RuntimeError(f"candidate runner not found: {args.candidate}; build it first")

    reference_runner = python_runner(canonical_reference)
    candidate = native_runner(args.candidate)
    timings: list[float] = []
    for _ in range(25):
        started = time.perf_counter_ns()
        candidate(development_bytes)
        timings.append((time.perf_counter_ns() - started) / 1_000_000.0)
    memory_mb = process_memory_mb()
    sorted_timings = sorted(timings)
    p95 = sorted_timings[min(len(sorted_timings) - 1, int(len(sorted_timings) * 0.95))]
    performance_metrics = {
        "latency_ms": p95,
        "memory_mb": memory_mb,
        "throughput": len(development_cases) / (statistics.fmean(timings) / 1000.0),
        "latency_p50_ms": statistics.median(timings),
        "latency_p95_ms": p95,
        "latency_max_ms": max(timings),
    }
    result = PromotionPipeline().evaluate(
        manifest,
        development_bytes,
        reference_runner=reference_runner,
        candidate_runner=candidate,
        divergence_path=ROOT / "validation" / "reports" / "episode_segmentation_v1_divergences.json",
        performance_metrics=performance_metrics,
    )
    if not result.equivalence_passed:
        raise RuntimeError(f"development equivalence failed: {result.divergences}")
    if not result.performance_passed:
        raise RuntimeError(f"performance gate failed: {result.performance}")

    reference_development = output_objects(reference_runner(development_bytes))
    development_outputs = output_objects(candidate(development_bytes))
    holdout_reference = reference_runner(holdout_bytes)
    holdout_candidate = candidate(holdout_bytes)
    holdout_divergences = compare_outputs(holdout_reference, holdout_candidate, manifest.data["equivalence"])
    holdout_reference_objects = output_objects(holdout_reference)
    holdout_outputs = output_objects(holdout_candidate)
    if holdout_divergences:
        raise RuntimeError(f"holdout equivalence failed: {holdout_divergences}")
    validate_episode_outputs(reference_development)
    validate_episode_outputs(development_outputs)
    validate_episode_outputs(holdout_reference_objects)
    validate_episode_outputs(holdout_outputs)
    if len(development_outputs) != len(development_cases) or len(holdout_outputs) != len(holdout_cases):
        raise RuntimeError("candidate output count does not match fixture count")
    invariant_failures_by_case = {
        case["case_id"]: invariant_failures(case, output)
        for case, output in zip(holdout_cases, holdout_outputs, strict=True)
    }
    invariant_failures_by_case.update(
        {
            case["case_id"]: invariant_failures(case, output)
            for case, output in zip(development_cases, development_outputs, strict=True)
        }
    )
    invariant_failures_flat = {
        case_id: failures for case_id, failures in invariant_failures_by_case.items() if failures
    }
    if invariant_failures_flat:
        raise RuntimeError(f"invariant gate failed: {invariant_failures_flat}")
    replay_first = candidate(development_bytes)
    replay_second = candidate(development_bytes)
    metamorphic = metamorphic_failures(development_cases, development_outputs)
    if replay_first != replay_second:
        metamorphic.append("same_input_produces_same_output")
    if metamorphic:
        raise RuntimeError(f"metamorphic gate failed: {metamorphic}")

    holdout_metrics = aggregate_metrics(holdout_cases, holdout_outputs)
    development_metrics = aggregate_metrics(development_cases, development_outputs)
    baseline_outputs = [canonical_reference({**case, "config": {**case["config"], "split_on_application_change": False, "split_on_document_change": False}}) for case in development_cases]
    ablation_outputs = [canonical_reference({**case, "config": {**case["config"], "split_on_application_change": False, "split_on_document_change": False}}) for case in development_cases]
    report = result.to_dict()
    report["scientific_evidence_boundary"] = "cross-language equivalence is computational verification, not ground truth"
    report["holdout"] = {
        "fixture_set": args.holdout.relative_to(ROOT).as_posix(),
        "sha256": canonical_sha256(args.holdout),
        "case_count": len(holdout_cases),
        "equivalence_passed": not holdout_divergences,
        "metrics": holdout_metrics,
    }
    report["reference"] = {
        "source_path": str(reference_path.relative_to(ROOT)).replace("\\", "/"),
        "source_sha256": reference_source_hash,
    }
    report["ground_truth"] = {
        "development": development_metrics,
        "holdout": holdout_metrics,
    }
    report["baseline"] = aggregate_metrics(development_cases, baseline_outputs)
    report["ablation"] = aggregate_metrics(development_cases, ablation_outputs)
    report["invariants"] = {"passed": True, "failures": invariant_failures_flat}
    report["metamorphic"] = {"passed": True, "failures": []}
    report["operational_measurement"] = {
        "metrics": performance_metrics,
        "interpretation": "operational only; POSIX child RSS or Windows validator working set; no cognitive claim",
    }
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(report, ensure_ascii=False, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(json.dumps({"promotion_id": manifest.data["promotion_id"], "equivalence": True, "holdout": True, "performance": result.performance}, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
