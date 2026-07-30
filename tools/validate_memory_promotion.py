"""Run SPEC-034 episodic-memory equivalence, holdout and scientific gates."""

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

from eu_digital_lab.episodic_memory import EpisodicMemory, MemoryQuery
from eu_digital_lab.promotion import PromotionManifest, PromotionPipeline, compare_outputs, python_runner


def cases(path: Path) -> list[dict[str, Any]]:
    return [json.loads(line) for line in path.read_text(encoding="utf-8").splitlines() if line.strip()]


def reference_transform(case: dict[str, Any]) -> dict[str, Any]:
    embeddings = {
        record["episode"]["episode_id"]: record.get("embedding")
        for record in case["records"]
    }

    def provider(episode: dict[str, Any]) -> list[float]:
        return list(embeddings.get(episode["episode_id"]) or [])

    memory = EpisodicMemory(
        embedding_provider=provider,
        max_episodes=int(case.get("max_episodes", 10000)),
    )
    store_results = []
    for record in case["records"]:
        store_results.append(memory.store(record["episode"]).value)
    consolidated = memory.consolidate() if case.get("consolidate", False) else []
    query_data = case.get("query", {})
    query = MemoryQuery(
        session_id=query_data.get("session_id"),
        applications=tuple(query_data.get("applications", [])),
        documents=tuple(query_data.get("documents", [])),
        modalities=tuple(query_data.get("modalities", [])),
        start_at=query_data.get("start_at"),
        end_at=query_data.get("end_at"),
        embedding=tuple(float(item) for item in query_data["embedding"]) if "embedding" in query_data else None,
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
        if episode_id not in records or item.get("provenance", {}).get("event_ids") != episode.get("event_ids"):
            failures.append("retrieval_provenance_matches_episode")
        if item.get("provenance", {}).get("created_by") != episode.get("created_by"):
            failures.append("retrieval_provenance_matches_episode")
        if not item.get("reason_codes") or "summary" not in episode or episode.get("summary") is not None:
            failures.append("no_summary_or_fact_is_generated")
    for relation in result.get("relations", []):
        left = records.get(relation.get("episode_a"), {})
        right = records.get(relation.get("episode_b"), {})
        expected = list(left.get("event_ids", [])) + list(right.get("event_ids", []))
        if relation.get("provenance", {}).get("event_ids") != expected:
            failures.append("relation_provenance_contains_source_events")
    if result.get("size", 0) > int(case.get("max_episodes", 10000)):
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
    if hashlib.sha256(development_bytes).hexdigest() != manifest.data["dataset"]["hash"]:
        raise RuntimeError("development fixture hash does not match manifest")
    if hashlib.sha256(holdout_bytes).hexdigest() != manifest.data["validation"]["holdout_hash"]:
        raise RuntimeError("holdout fixture hash does not match manifest")
    if not args.candidate.exists():
        raise RuntimeError(f"candidate runner not found: {args.candidate}")
    reference = python_runner(reference_transform)
    candidate = run_candidate(args.candidate)
    timings: list[float] = []
    for _ in range(25):
        started = time.perf_counter_ns()
        candidate(development_bytes)
        timings.append((time.perf_counter_ns() - started) / 1_000_000.0)
    rss_mb = resource.getrusage(resource.RUSAGE_CHILDREN).ru_maxrss / 1024.0
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
        case_failures = invariant_failures(case, output)
        if case_failures:
            failures[case["case_id"]] = case_failures
    if failures:
        raise RuntimeError(f"invariant gate failed: {failures}")
    report = result.to_dict()
    report["scientific_evidence_boundary"] = "cross-language equivalence is computational verification, not ground truth"
    report["holdout"] = {
        "fixture_set": str(args.holdout.relative_to(ROOT)),
        "sha256": hashlib.sha256(holdout_bytes).hexdigest(),
        "case_count": len(holdout_cases),
        "equivalence_passed": not holdout_divergences,
        "metrics": metric_summary(holdout_cases, holdout_outputs),
    }
    report["treatment_metrics"] = metric_summary(development_cases, development_outputs)
    report["baseline_metrics"] = metric_summary(development_cases, [chronological_baseline(case) for case in development_cases])
    report["ablation_metrics"] = report["baseline_metrics"]
    report["invariants"] = {"passed": True, "failures": failures}
    report["operational_measurement"] = {"metrics": metrics, "interpretation": "operational only; no cognitive claim"}
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(report, ensure_ascii=False, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(json.dumps({"promotion_id": manifest.data["promotion_id"], "equivalence": True, "holdout": True, "performance": result.performance}, indent=2))
    return 0


def result_output(value: bytes) -> list[str]:
    return [line for line in value.decode("utf-8").splitlines() if line.strip()]


if __name__ == "__main__":
    raise SystemExit(main())
