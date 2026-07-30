"""Run SPEC-037 global-workspace equivalence, holdout and scientific gates."""

from __future__ import annotations

import argparse
import asyncio
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

from eu_digital_lab.global_workspace import (
    BASELINE_ID,
    GlobalWorkspace,
    WorkspaceCandidate,
    WorkspaceConfig,
    evaluate_selection,
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


def workspace_config(
    case: dict[str, Any], override: dict[str, Any] | None = None
) -> WorkspaceConfig:
    values = dict(case.get("config", {}))
    if override:
        values.update(override)
    return WorkspaceConfig(**values)


def reference_transform(
    case: dict[str, Any], *, config_override: dict[str, Any] | None = None
) -> dict[str, Any]:
    workspace = GlobalWorkspace(
        case["workspace_id"],
        case["session_id"],
        workspace_config(case, config_override),
    )
    snapshots: list[dict[str, Any]] = []
    broadcasts: list[dict[str, Any]] = []
    for operation in case["operations"]:
        operation_type = operation["type"]
        now = operation.get("now")
        if operation_type == "admit":
            snapshot = workspace.admit(
                WorkspaceCandidate(**operation["candidate"]),
                now,
            )
            snapshots.append(snapshot.to_mapping())
        elif operation_type == "snapshot":
            snapshots.append(workspace.snapshot(now).to_mapping())
        elif operation_type == "update_priority":
            snapshots.append(
                workspace.update_priority(
                    operation["candidate_id"],
                    operation["priority"],
                    now,
                ).to_mapping()
            )
        elif operation_type == "broadcast":
            if not snapshots:
                raise ValueError("workspace broadcast requires a preceding snapshot")

            async def publish(event: dict[str, Any]) -> None:
                broadcasts.append(event)

            snapshot = workspace.snapshot(now) if False else snapshots[-1]
            event = asyncio.run(
                workspace.broadcast(
                    _snapshot_from_mapping(snapshot),
                    publish,
                    operation.get("emitted_at", now),
                )
            )
            if broadcasts and broadcasts[-1] == event:
                continue
            broadcasts.append(event)
        else:
            raise ValueError(
                f"unsupported global-workspace operation: {operation_type}"
            )
    return {"broadcasts": broadcasts, "schema_version": "1.0", "snapshots": snapshots}


def _snapshot_from_mapping(value: dict[str, Any]) -> Any:
    """Recreate the immutable reference snapshot for the injected broadcast port."""

    from eu_digital_lab.global_workspace import (
        SalienceAssessment,
        SelectionDecision,
        WorkspaceItem,
        WorkspaceSnapshot,
    )

    active_items = tuple(
        WorkspaceItem(
            workspace_item_id=item["workspace_item_id"],
            schema_version=item["schema_version"],
            workspace_id=item["workspace_id"],
            candidate_id=item["candidate_id"],
            session_id=item["session_id"],
            source_kind=item["source_kind"],
            source_refs=tuple(item["source_refs"]),
            observed_at=item["observed_at"],
            admitted_at=item["admitted_at"],
            expires_at=item["expires_at"],
            content=item["content"],
            salience=SalienceAssessment(
                policy_id=item["salience"]["policy_id"],
                score=item["salience"]["score"],
                observed_factors=item["salience"]["observed_factors"],
                missing_factors=tuple(item["salience"]["missing_factors"]),
            ),
            snapshot_id=item["selection"]["snapshot_id"],
            rank=item["selection"]["rank"],
            selected_at=item["selection"]["selected_at"],
            selection_reasons=tuple(item["selection"]["reasons"]),
        )
        for item in value["active_items"]
    )
    decisions = tuple(
        SelectionDecision(
            candidate_id=item["candidate_id"],
            score=item["score"],
            selected=item["selected"],
            rank=item["rank"],
            reason_codes=tuple(item["reason_codes"]),
        )
        for item in value["decisions"]
    )
    return WorkspaceSnapshot(
        snapshot_id=value["snapshot_id"],
        schema_version=value["schema_version"],
        workspace_id=value["workspace_id"],
        session_id=value["session_id"],
        created_at=value["created_at"],
        capacity=value["capacity"],
        policy_id=value["policy_id"],
        config_fingerprint=value["config_fingerprint"],
        selection_churn=value["selection_churn"],
        active_items=active_items,
        decisions=decisions,
        expired_candidate_ids=tuple(value["expired_candidate_ids"]),
        discarded_candidate_ids=tuple(value["discarded_candidate_ids"]),
    )


def run_candidate(binary: Path):
    def runner(fixture: bytes) -> bytes:
        candidate = binary
        if not candidate.exists() and os.name == "nt" and candidate.suffix == "":
            candidate = candidate.with_suffix(".exe")
        completed = subprocess.run(
            [str(candidate), "--global-workspace"],
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
    for snapshot in result.get("snapshots", []):
        if len(snapshot["active_items"]) > snapshot["capacity"]:
            failures.append("capacity_bounded")
        if not 0.0 <= float(snapshot["selection_churn"]) <= 1.0:
            failures.append("selection_churn_bounded")
        for item in snapshot["active_items"]:
            if not item["source_refs"] or item["session_id"] != case["session_id"]:
                failures.append("provenance_preserved")
            if any(
                term in json.dumps(item["content"], sort_keys=True).lower()
                for term in ("fact", "plan", "action", "task")
            ):
                failures.append("workspace_does_not_create_facts_or_plans")
            validate_shared_schema(item, "workspace_item.schema.json")
        validate_shared_schema(snapshot, "workspace_snapshot.schema.json")
        for decision in snapshot["decisions"]:
            if (
                decision["score"] is not None
                and not 0.0 <= float(decision["score"]) <= 1.0
            ):
                failures.append("score_bounded")
    for event in result.get("broadcasts", []):
        validate_shared_schema(event["payload"], "workspace_broadcast.schema.json")
        if event["source"] != "global_workspace" or event["privacy_class"] != "local":
            failures.append("broadcast_local")
        if event["event_type"] != "workspace.selection.v1":
            failures.append("broadcast_versioned")
    declared = set(case.get("invariants", []))
    if "expiration_is_explicit" in declared and result["snapshots"][-1][
        "expired_candidate_ids"
    ] != [
        "earlier-id",
        "later-id",
    ]:
        failures.append("expiration_is_explicit")
    if (
        "discarded_is_explicit" in declared
        and not result["snapshots"][-1]["discarded_candidate_ids"]
    ):
        failures.append("discarded_is_explicit")
    return sorted(set(failures))


def selection_metrics(
    cases_value: list[dict[str, Any]], outputs: list[dict[str, Any]]
) -> dict[str, float]:
    metrics = [
        evaluate_selection(
            _snapshot_from_mapping(output["snapshots"][-1]), case.get("relevance", [])
        )
        for case, output in zip(cases_value, outputs, strict=True)
    ]
    return {
        "precision_mean": statistics.fmean(
            float(item["precision_at_capacity"]) for item in metrics
        ),
        "recall_mean": statistics.fmean(
            float(item["recall_at_capacity"]) for item in metrics
        ),
        "f1_mean": statistics.fmean(float(item["selection_f1"]) for item in metrics),
        "churn_mean": statistics.fmean(
            float(item["selection_churn"]) for item in metrics
        ),
        "occupancy_mean": statistics.fmean(
            float(item["occupancy"]) for item in metrics
        ),
        "case_count": float(len(metrics)),
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
        default=ROOT / "validation" / "equivalence" / "global_workspace_v1.jsonl",
    )
    parser.add_argument(
        "--holdout",
        type=Path,
        default=ROOT / "validation" / "holdout" / "global_workspace_v1_holdout.jsonl",
    )
    parser.add_argument(
        "--manifest",
        type=Path,
        default=ROOT / "promotions" / "cognition.global_workspace.v1.json",
    )
    parser.add_argument(
        "--report",
        type=Path,
        default=ROOT / "validation" / "reports" / "global_workspace_v1.json",
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
    performance_metrics = {
        "latency_ms": statistics.quantiles(timings, n=20, method="inclusive")[18],
        "latency_p50_ms": statistics.median(timings),
        "latency_p95_ms": statistics.quantiles(timings, n=20, method="inclusive")[18],
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
        / "global_workspace_v1_divergences.json",
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

    baseline_outputs = [
        reference_transform(case, config_override={"selection_policy": BASELINE_ID})
        for case in development_cases
    ]
    ablation_outputs = [
        reference_transform(
            case,
            config_override={
                "capacity": max(1, int(case.get("config", {}).get("capacity", 4)) - 1),
                "selection_policy": BASELINE_ID,
            },
        )
        for case in development_cases
    ]
    treatment = selection_metrics(development_cases, development_outputs)
    baseline = selection_metrics(development_cases, baseline_outputs)
    ablation = selection_metrics(development_cases, ablation_outputs)
    if treatment["f1_mean"] <= baseline["f1_mean"]:
        raise RuntimeError("treatment does not beat FIFO on development annotations")

    report = result.to_dict()
    report.update(
        {
            "scientific_evidence_boundary": "equivalence and holdout are computational verification, not ground truth",
            "holdout": {
                "fixture_set": args.holdout.relative_to(ROOT).as_posix(),
                "sha256": canonical_sha256(args.holdout),
                "case_count": len(holdout_cases),
                "equivalence_passed": not holdout_divergences,
                "metrics": selection_metrics(holdout_cases, holdout_outputs),
            },
            "baseline": {
                "policy": BASELINE_ID,
                **baseline,
                "interpretation": "operational baseline only",
            },
            "ablation": {
                "policy": "capacity_minus_one_fifo",
                **ablation,
                "interpretation": "operational ablation only",
            },
            "treatment_metrics": {"policy": "observed_weighted_mean_v1", **treatment},
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
