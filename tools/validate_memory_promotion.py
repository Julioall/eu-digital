"""Run SPEC-034 episodic-memory equivalence, holdout and scientific gates."""

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
from collections.abc import Callable, Mapping, Sequence
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

from eu_digital_lab.episodic_memory import EpisodicMemory, MemoryQuery
from eu_digital_lab.promotion import (
    PromotionManifest,
    PromotionPipeline,
    compare_outputs,
    python_runner,
)
from eu_digital_lab.schema_validation import validate_shared_schema


def cases(path: Path) -> list[dict[str, Any]]:
    return [json.loads(line) for line in path.read_text(encoding="utf-8").splitlines() if line.strip()]


def canonical_sha256(path: Path) -> str:
    """Hash fixture content independently of Git's platform newline conversion."""

    return hashlib.sha256(path.read_bytes().replace(b"\r\n", b"\n")).hexdigest()


def output_objects(value: bytes) -> list[dict[str, Any]]:
    return [json.loads(line) for line in value.splitlines() if line.strip()]


def validate_episode_records(records: list[dict[str, Any]]) -> None:
    for record in records:
        validate_shared_schema(record["episode"], "episode.schema.json")


def validate_memory_outputs(outputs: list[dict[str, Any]]) -> None:
    for result in outputs:
        for item in result.get("retrieval", []):
            validate_shared_schema(item["episode"], "episode.schema.json")


def current_process_memory_mb() -> float:
    if resource is not None:
        value = resource.getrusage(resource.RUSAGE_CHILDREN).ru_maxrss
        return value / (1024.0 * 1024.0) if platform.system() == "Darwin" else value / 1024.0
    if platform.system() == "Windows":
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
        if ctypes.windll.psapi.GetProcessMemoryInfo(process, ctypes.byref(counters), counters.cb):
            return counters.WorkingSetSize / (1024.0 * 1024.0)
        minimum = ctypes.c_size_t()
        maximum = ctypes.c_size_t()
        get_working_set = ctypes.windll.kernel32.GetProcessWorkingSetSize
        get_working_set.argtypes = [wintypes.HANDLE, ctypes.POINTER(ctypes.c_size_t), ctypes.POINTER(ctypes.c_size_t)]
        get_working_set.restype = wintypes.BOOL
        if get_working_set(process, ctypes.byref(minimum), ctypes.byref(maximum)):
            return minimum.value / (1024.0 * 1024.0)
    return 0.0


def reference_transform(
    case: dict[str, Any], *, disable_context: bool = False, disable_embedding: bool = False
) -> dict[str, Any]:
    provider: Callable[[Mapping[str, Any]], Sequence[float]] | None = None
    if not disable_embedding:
        embeddings = {
            record["episode"]["episode_id"]: record.get("embedding")
            for record in case["records"]
        }

        def embedding_provider(episode: Mapping[str, Any]) -> Sequence[float]:
            return list(embeddings.get(episode["episode_id"]) or [])

        provider = embedding_provider

    memory = EpisodicMemory(
        embedding_provider=provider,
        max_episodes=int(case.get("max_episodes", 10000)),
    )
    store_results = []
    for record in case["records"]:
        store_results.append(memory.store(record["episode"]).value)
    consolidated = memory.consolidate() if case.get("consolidate", False) else []
    query_data = {} if disable_context else case.get("query", {})
    query = MemoryQuery(
        session_id=query_data.get("session_id"),
        applications=tuple(query_data.get("applications", [])),
        documents=tuple(query_data.get("documents", [])),
        modalities=tuple(query_data.get("modalities", [])),
        start_at=query_data.get("start_at"),
        end_at=query_data.get("end_at"),
        embedding=(
            tuple(float(item) for item in query_data["embedding"])
            if "embedding" in query_data and not disable_embedding
            else None
        ),
        limit=int(query_data.get("limit", 10)),
    )
    retrieval = []
    for item in memory.retrieve(query):
        retrieval.append(
            {
                "episode": item.episode,
                "score": item.score,
                "reason_codes": list(item.reason_codes),
                "explanation": item.explanation,
                "provenance": item.provenance,
            }
        )
    relations = [
        {
            "episode_a": item["episode_a"],
            "episode_b": item["episode_b"],
            "score": item["score"],
            "reason_codes": item["reason_codes"],
            "provenance": item["provenance"],
        }
        for item in memory.similarity_relations(float(case.get("minimum_relation_score", 0.0)))
    ]
    return {
        "schema_version": "1.0",
        "store_results": store_results,
        "size": memory.size(),
        "consolidated": consolidated,
        "retrieval": retrieval,
        "relations": relations,
    }


def invariant_failures(case: dict[str, Any], result: dict[str, Any]) -> list[str]:
    failures: list[str] = []
    records = {record["episode"]["episode_id"]: record["episode"] for record in case["records"]}
    for item in result.get("retrieval", []):
        episode = item.get("episode", {})
        episode_id = episode.get("episode_id")
        provenance = item.get("provenance", {})
        if episode_id not in records or provenance.get("event_ids") != episode.get("event_ids"):
            failures.append("retrieval_provenance_matches_episode")
        if provenance.get("episode_id") != episode_id or provenance.get("created_by") != episode.get("created_by"):
            failures.append("retrieval_provenance_matches_episode")
        if provenance.get("schema_version") != episode.get("schema_version"):
            failures.append("retrieval_provenance_matches_episode")
        if not item.get("reason_codes") or not item.get("explanation") or "summary" not in episode or episode.get("summary") is not None:
            failures.append("no_summary_or_fact_is_generated")
        if episode_id not in records:
            continue
        if (
            case.get("query", {}).get("embedding")
            and not any(record.get("embedding") for record in case["records"])
            and "embedding.cosine" in item.get("reason_codes", [])
        ):
            failures.append("missing_embedding_does_not_become_negative_evidence")
    for relation in result.get("relations", []):
        left_id = relation.get("episode_a")
        right_id = relation.get("episode_b")
        left = records.get(left_id, {})
        right = records.get(right_id, {})
        expected = list(left.get("event_ids", [])) + list(right.get("event_ids", []))
        if left_id == right_id or not left or not right or relation.get("provenance", {}).get("event_ids") != expected:
            failures.append("relation_provenance_contains_source_events")
        if not relation.get("reason_codes"):
            failures.append("relation_has_explicit_reason")
    consolidated = result.get("consolidated", [])
    if any(episode_id not in records for episode_id in consolidated):
        failures.append("retention_is_bounded_and_deterministic")
    if case.get("consolidate") and result.get("size", 0) > int(case.get("max_episodes", 10000)):
        failures.append("retention_is_bounded_and_deterministic")
    expected_duplicates = sum(1 for item in result.get("store_results", []) if item == "duplicate")
    input_duplicates = len(case["records"]) - len(records)
    if expected_duplicates != input_duplicates:
        failures.append("stored_episode_is_immutable_by_duplicate_id")
    return sorted(set(failures))


def metric_summary(test_cases: list[dict[str, Any]], outputs: list[dict[str, Any]]) -> dict[str, float]:
    recalls: list[float] = []
    reciprocal_ranks: list[float] = []
    provenance_precisions: list[float] = []
    for case, output in zip(test_cases, outputs, strict=True):
        relevant = list(case.get("relevance", []))
        predicted = [item["episode"]["episode_id"] for item in output.get("retrieval", [])]
        top = predicted[: int(case.get("query", {}).get("limit", 10))]
        recalls.append(len(set(top) & set(relevant)) / len(relevant) if relevant else (1.0 if not top else 0.0))
        rank = next((index + 1 for index, item in enumerate(top) if item in relevant), None)
        reciprocal_ranks.append(1.0 / rank if rank else 0.0)
        valid = [
            item.get("provenance", {}).get("event_ids") == item.get("episode", {}).get("event_ids")
            for item in output.get("retrieval", [])
        ]
        provenance_precisions.append(sum(valid) / len(valid) if valid else 1.0)
    return {
        "recall_at_k": statistics.fmean(recalls),
        "mrr": statistics.fmean(reciprocal_ranks),
        "provenance_precision": statistics.fmean(provenance_precisions),
    }


def chronological_baseline(case: dict[str, Any]) -> dict[str, Any]:
    unique: dict[str, dict[str, Any]] = {}
    for record in case["records"]:
        unique.setdefault(record["episode"]["episode_id"], record["episode"])
    ordered = sorted(unique.values(), key=lambda episode: (episode["start_at"], episode["episode_id"]))
    limit = int(case.get("query", {}).get("limit", 10))
    return {"retrieval": [{"episode": episode, "provenance": {"event_ids": episode["event_ids"]}} for episode in ordered[:limit]]}


def run_candidate(binary: Path):
    def runner(fixture: bytes) -> bytes:
        completed = subprocess.run([str(binary), "--episodic-memory"], input=fixture, capture_output=True, check=False)
        if completed.returncode:
            raise RuntimeError(completed.stderr.decode("utf-8", errors="replace"))
        return completed.stdout

    return runner


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--candidate", type=Path, default=ROOT / "build" / "dev" / "promotion_fixture_runner")
    parser.add_argument("--fixture", type=Path, default=ROOT / "validation" / "equivalence" / "episodic_memory_v1.jsonl")
    parser.add_argument("--holdout", type=Path, default=ROOT / "validation" / "holdout" / "episodic_memory_v1_holdout.jsonl")
    parser.add_argument("--manifest", type=Path, default=ROOT / "promotions" / "cognition.episodic_memory.v1.json")
    parser.add_argument("--report", type=Path, default=ROOT / "validation" / "reports" / "episodic_memory_v1.json")
    args = parser.parse_args()
    manifest = PromotionManifest.load(args.manifest)
    development_bytes = args.fixture.read_bytes()
    holdout_bytes = args.holdout.read_bytes()
    development_cases = cases(args.fixture)
    holdout_cases = cases(args.holdout)
    if canonical_sha256(args.fixture) != manifest.data["dataset"]["hash"]:
        raise RuntimeError("development fixture hash does not match manifest")
    if canonical_sha256(args.holdout) != manifest.data["validation"]["holdout_hash"]:
        raise RuntimeError("holdout fixture hash does not match manifest")
    if len(development_cases) != int(manifest.data["dataset"]["case_count"]):
        raise RuntimeError("development case count does not match frozen manifest")
    development_ids = {str(case["case_id"]) for case in development_cases}
    holdout_ids = {str(case["case_id"]) for case in holdout_cases}
    if not development_ids.isdisjoint(holdout_ids):
        raise RuntimeError("development and holdout case IDs overlap")
    for case in development_cases + holdout_cases:
        validate_episode_records(case["records"])
    reference_data = manifest.data["reference"]
    reference_path = ROOT / str(reference_data.get("source_path", "python/eu_digital_lab/episodic_memory.py"))
    if not reference_path.exists():
        raise RuntimeError(f"frozen Python reference source not found: {reference_path}")
    reference_source_hash = canonical_sha256(reference_path)
    if reference_source_hash != reference_data.get("source_sha256"):
        raise RuntimeError("Python reference source hash does not match frozen manifest")
    if not args.candidate.exists() and os.name == "nt" and args.candidate.suffix == "":
        windows_candidate = args.candidate.with_suffix(".exe")
        if windows_candidate.exists():
            args.candidate = windows_candidate
    if not args.candidate.exists():
        raise RuntimeError(f"candidate runner not found: {args.candidate}")
    reference = python_runner(reference_transform)
    candidate = run_candidate(args.candidate)
    timings: list[float] = []
    for _ in range(25):
        started = time.perf_counter_ns()
        candidate(development_bytes)
        timings.append((time.perf_counter_ns() - started) / 1_000_000.0)
    rss_mb = current_process_memory_mb()
    sorted_timings = sorted(timings)
    p95 = sorted_timings[min(len(sorted_timings) - 1, int(len(sorted_timings) * 0.95))]
    metrics = {
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
        divergence_path=ROOT / "validation" / "reports" / "episodic_memory_v1_divergences.json",
        performance_metrics=metrics,
    )
    if not result.equivalence_passed or not result.performance_passed:
        raise RuntimeError(f"development gate failed: equivalence={result.equivalence_passed}, performance={result.performance}")
    holdout_reference = reference(holdout_bytes)
    holdout_candidate = candidate(holdout_bytes)
    holdout_divergences = compare_outputs(holdout_reference, holdout_candidate, manifest.data["equivalence"])
    if holdout_divergences:
        raise RuntimeError(f"holdout gate failed: {holdout_divergences}")
    development_reference_objects = output_objects(reference(development_bytes))
    development_outputs = output_objects(candidate(development_bytes))
    holdout_reference_objects = output_objects(holdout_reference)
    holdout_outputs = output_objects(holdout_candidate)
    if len(development_outputs) != len(development_cases) or len(holdout_outputs) != len(holdout_cases):
        raise RuntimeError("candidate output count does not match fixture count")
    validate_memory_outputs(development_reference_objects)
    validate_memory_outputs(development_outputs)
    validate_memory_outputs(holdout_reference_objects)
    validate_memory_outputs(holdout_outputs)
    failures = {}
    for case, output in zip(development_cases + holdout_cases, development_outputs + holdout_outputs, strict=True):
        case_failures = invariant_failures(case, output)
        if case_failures:
            failures[case["case_id"]] = case_failures
    if failures:
        raise RuntimeError(f"invariant gate failed: {failures}")
    replay_first = candidate(development_bytes)
    replay_second = candidate(development_bytes)
    if replay_first != replay_second:
        raise RuntimeError("metamorphic gate failed: same input produced different outputs")
    report = result.to_dict()
    report["scientific_evidence_boundary"] = "cross-language equivalence is computational verification, not ground truth"
    report["holdout"] = {
        "fixture_set": args.holdout.relative_to(ROOT).as_posix(),
        "sha256": canonical_sha256(args.holdout),
        "case_count": len(holdout_cases),
        "equivalence_passed": not holdout_divergences,
        "metrics": metric_summary(holdout_cases, holdout_outputs),
    }
    report["reference"] = {
        "source_path": reference_path.relative_to(ROOT).as_posix(),
        "source_sha256": reference_source_hash,
    }
    report["ground_truth"] = {
        "development": metric_summary(development_cases, development_outputs),
        "holdout": metric_summary(holdout_cases, holdout_outputs),
    }
    report["treatment_metrics"] = metric_summary(development_cases, development_outputs)
    report["baseline_metrics"] = metric_summary(
        development_cases, [chronological_baseline(case) for case in development_cases]
    )
    report["ablation_metrics"] = metric_summary(
        development_cases,
        [reference_transform(case, disable_context=True, disable_embedding=True) for case in development_cases],
    )
    report["invariants"] = {"passed": True, "failures": failures}
    report["metamorphic"] = {"passed": True, "failures": []}
    report["operational_measurement"] = {
        "metrics": metrics,
        "interpretation": "operational only; POSIX child RSS or Windows validator working set; no cognitive claim",
    }
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(report, ensure_ascii=False, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(json.dumps({"promotion_id": manifest.data["promotion_id"], "equivalence": True, "holdout": True, "performance": result.performance}, indent=2))
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
