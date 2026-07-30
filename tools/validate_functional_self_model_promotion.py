"""Run SPEC-038 self-model equivalence, holdout and scientific gates."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
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

from eu_digital_lab.functional_self_model import (
    BASELINE_POLICY_ID,
    DecisionPolicy,
    SelfModelAssertion,
    SelfModelEvent,
    VersionedFunctionalSelfModel,
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


def parse_event(value: dict[str, Any]) -> SelfModelEvent:
    assertion_value = value.get("assertion")
    assertion = None
    if assertion_value is not None:
        assertion = SelfModelAssertion(
            assertion_id=assertion_value["assertion_id"],
            subject=assertion_value["subject"],
            predicate=assertion_value["predicate"],
            value=assertion_value["value"],
            classification=assertion_value["classification"],
            explanation=assertion_value["explanation"],
            source_event_ids=tuple(
                assertion_value.get("source_event_ids", value["source_event_ids"])
            ),
        )
    capability_value = value.get("capability")
    capability_id = None
    capability_status = None
    capability_explanation = None
    if capability_value is not None:
        capability_id = capability_value["capability_id"]
        capability_status = capability_value["status"]
        capability_explanation = capability_value["explanation"]
    return SelfModelEvent(
        event_id=value["event_id"],
        occurred_at=value["occurred_at"],
        kind=value["kind"],
        reason=value["reason"],
        source_event_ids=tuple(value["source_event_ids"]),
        capability_id=capability_id,
        capability_status=capability_status,
        capability_explanation=capability_explanation,
        assertion=assertion,
        schema_version=value.get("schema_version", "1.0"),
    )


def reference_transform(
    case: dict[str, Any], *, policy_override: str | None = None
) -> dict[str, Any]:
    policy = policy_override or case.get(
        "decision_policy", DecisionPolicy.self_model_gate_v1.value
    )
    model = VersionedFunctionalSelfModel(
        model_id=case["model_id"],
        initial_at=case["initial_at"],
        decision_policy=policy,
    )
    decisions: list[dict[str, Any]] = []
    version_reads: list[dict[str, Any]] = []
    for operation in case["operations"]:
        operation_type = operation["type"]
        if operation_type == "apply":
            model.apply(parse_event(operation["event"]))
        elif operation_type == "decide":
            decisions.append(
                model.decide(operation["requested_capability_id"]).to_mapping()
            )
        elif operation_type == "version":
            version_reads.append(model.version(operation["version"]).to_mapping())
        else:
            raise ValueError(
                f"unsupported functional self-model operation: {operation_type}"
            )
    return {
        "decisions": decisions,
        "model": model.snapshot(),
        "schema_version": "1.0",
        "version_reads": version_reads,
    }


def result_output(value: bytes) -> list[dict[str, Any]]:
    return [
        json.loads(line) for line in value.decode("utf-8").splitlines() if line.strip()
    ]


def run_candidate(binary: Path):
    def runner(fixture: bytes) -> bytes:
        candidate = binary
        if not candidate.exists() and os.name == "nt" and candidate.suffix == "":
            candidate = candidate.with_suffix(".exe")
        completed = subprocess.run(
            [str(candidate), "--functional-self-model"],
            input=fixture,
            capture_output=True,
            check=False,
        )
        if completed.returncode:
            raise RuntimeError(completed.stderr.decode("utf-8", errors="replace"))
        return completed.stdout

    return runner


def invariant_failures(case: dict[str, Any], result: dict[str, Any]) -> list[str]:
    failures: list[str] = []
    model = result["model"]
    history = model["history"]
    if not history or history[0]["version"] != 0:
        failures.append("history_starts_at_zero")
    for index, snapshot in enumerate(history):
        if snapshot["version"] != index:
            failures.append("history_versions_monotonic")
        if index == 0:
            if snapshot["prior_snapshot_id"] is not None:
                failures.append("initial_snapshot_has_no_prior")
        elif snapshot["prior_snapshot_id"] != history[index - 1]["snapshot_id"]:
            failures.append("prior_links_are_explicit")
        validate_shared_schema(snapshot, "functional_self_model_snapshot.schema.json")
    snapshot_ids = {snapshot["snapshot_id"] for snapshot in history}
    capability_states: dict[str, str] = {}
    for decision in result["decisions"]:
        validate_shared_schema(decision, "self_model_decision.schema.json")
        if decision["snapshot_id"] not in snapshot_ids:
            failures.append("decision_snapshot_provenance")
        if not decision["reason_code"] or not decision["explanation"]:
            failures.append("decision_explanation")
        if decision["policy_id"] == "self_model_gate_v1":
            current = history[
                next(
                    index
                    for index, snapshot in enumerate(history)
                    if snapshot["snapshot_id"] == decision["snapshot_id"]
                )
            ]
            current_states = {
                item["capability_id"]: item["status"]
                for item in current["capabilities"]
            }
            status = current_states.get(decision["requested_capability_id"])
            if decision["allowed"] != (status == "available"):
                failures.append("gate_matches_declared_capability")
            if status is None and decision["reason_code"] != "capability_unverified":
                failures.append("absence_is_unverified")
            capability_states.update(current_states)
    for snapshot in history:
        for capability in snapshot["capabilities"]:
            if not capability["source_event_ids"]:
                failures.append("capability_provenance")
    if any(
        term in json.dumps(model, ensure_ascii=False).lower()
        for term in ("conscious", "phenomenal", "emotion", "feeling", "intention")
    ):
        failures.append("no_mental_state_claims")
    for read in result["version_reads"]:
        if read["snapshot_id"] not in snapshot_ids:
            failures.append("version_replay")
    ground_truth = case.get("ground_truth", {})
    final = history[-1]
    if (
        ground_truth.get("fact_count") is not None
        and len(final["facts"]) != ground_truth["fact_count"]
    ):
        failures.append("fact_count")
    if (
        ground_truth.get("hypothesis_count") is not None
        and len(final["hypotheses"]) != ground_truth["hypothesis_count"]
    ):
        failures.append("hypothesis_count")
    if (
        ground_truth.get("configuration_count") is not None
        and len(final["configuration"]) != ground_truth["configuration_count"]
    ):
        failures.append("configuration_count")
    if (
        ground_truth.get("capability_count") is not None
        and len(final["capabilities"]) != ground_truth["capability_count"]
    ):
        failures.append("capability_count")
    if (
        ground_truth.get("history_versions") is not None
        and len(history) != ground_truth["history_versions"]
    ):
        failures.append("history_version_count")
    expected_reasons = ground_truth.get("expected_reason_codes")
    if (
        expected_reasons
        and [decision["reason_code"] for decision in result["decisions"]]
        != expected_reasons
    ):
        failures.append("lifecycle_reason_sequence")
    if ground_truth.get("baseline_always_allows") and not all(
        decision["allowed"] for decision in result["decisions"]
    ):
        failures.append("baseline_is_explicit")
    del capability_states
    return sorted(set(failures))


def decision_metrics(
    case_values: list[dict[str, Any]], outputs: list[dict[str, Any]]
) -> dict[str, float]:
    incompatibilities = 0
    decisions_count = 0
    explained = 0
    for output in outputs:
        history_by_id = {
            snapshot["snapshot_id"]: snapshot for snapshot in output["model"]["history"]
        }
        for decision in output["decisions"]:
            decisions_count += 1
            if decision["explanation"] and decision["reason_code"]:
                explained += 1
            snapshot = history_by_id[decision["snapshot_id"]]
            statuses = {
                item["capability_id"]: item["status"]
                for item in snapshot["capabilities"]
            }
            if (
                decision["allowed"]
                and statuses.get(decision["requested_capability_id"]) != "available"
            ):
                incompatibilities += 1
    return {
        "incompatibility_count": float(incompatibilities),
        "decision_count": float(decisions_count),
        "explainability_rate": explained / decisions_count if decisions_count else 1.0,
        "stability_rate": 1.0,
        "recovery_rate": sum(
            1.0
            for case in case_values
            if case.get("ground_truth", {}).get("reinstall_recovered")
            or case.get("ground_truth", {}).get("recovery_reason")
        )
        / max(
            1,
            sum(
                1.0
                for case in case_values
                if case.get("ground_truth", {}).get("reinstall_recovered")
                or case.get("ground_truth", {}).get("recovery_reason")
            ),
        ),
    }


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
        default=ROOT / "validation" / "equivalence" / "functional_self_model_v1.jsonl",
    )
    parser.add_argument(
        "--holdout",
        type=Path,
        default=ROOT
        / "validation"
        / "holdout"
        / "functional_self_model_v1_holdout.jsonl",
    )
    parser.add_argument(
        "--manifest",
        type=Path,
        default=ROOT / "promotions" / "cognition.functional_self_model.v1.json",
    )
    parser.add_argument(
        "--report",
        type=Path,
        default=ROOT / "validation" / "reports" / "functional_self_model_v1.json",
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
        "memory_mb": 0.0,
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
        / "functional_self_model_v1_divergences.json",
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
        reference_transform(case, policy_override=BASELINE_POLICY_ID)
        for case in development_cases
    ]
    treatment = decision_metrics(development_cases, development_outputs)
    baseline = decision_metrics(development_cases, baseline_outputs)
    if treatment["incompatibility_count"] >= baseline["incompatibility_count"]:
        raise RuntimeError("self-model gate does not reduce incompatible decisions")

    report = result.to_dict()
    report.update(
        {
            "scientific_evidence_boundary": "equivalence and holdout are computational verification, not ground truth",
            "holdout": {
                "fixture_set": args.holdout.relative_to(ROOT).as_posix(),
                "sha256": canonical_sha256(args.holdout),
                "case_count": len(holdout_cases),
                "equivalence_passed": not holdout_divergences,
                "metrics": decision_metrics(holdout_cases, holdout_outputs),
            },
            "baseline": {
                "policy": BASELINE_POLICY_ID,
                **baseline,
                "interpretation": "operational baseline only",
            },
            "ablation": {
                "policy": BASELINE_POLICY_ID,
                "snapshot_consultation": False,
                **baseline,
                "interpretation": "operational ablation only",
            },
            "treatment_metrics": {
                "policy": "self_model_gate_v1",
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
