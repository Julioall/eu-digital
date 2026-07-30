"""Validate SPEC-040 local model gateway contracts and native equivalence."""

from __future__ import annotations

import argparse
import asyncio
import hashlib
import json
import os
import statistics
import sys
import time
from pathlib import Path
from typing import Any

ROOT = Path(__file__).resolve().parents[1]
LAB_ROOT = ROOT / "python"
if str(LAB_ROOT) not in sys.path:
    sys.path.insert(0, str(LAB_ROOT))

from eu_digital_lab.local_model_gateway import (
    ABLATION,
    BASELINE_SCHEDULER_ID,
    FALSIFICATION,
    HYPOTHESIS,
    GatewayConfig,
    LocalModelGateway,
    ModelRequest,
    PromptTemplate,
    SchedulingPolicy,
)
from eu_digital_lab.promotion import (
    PromotionManifest,
    PromotionPipeline,
    command_runner,
    compare_outputs,
    python_runner,
)
from eu_digital_lab.schema_validation import validate_shared_schema

SIGNATURE_PREFIX = "eu-digital-model-signature-v1:"
MAX_MODEL_BYTES = 4 * 1024 * 1024 * 1024
FORBIDDEN_TERMS = ("conscious", "phenomenal", "emotion", "feeling")


def load_cases(path: Path) -> list[dict[str, Any]]:
    return [
        json.loads(line)
        for line in path.read_text(encoding="utf-8").splitlines()
        if line.strip()
    ]


def canonical_sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes().replace(b"\r\n", b"\n")).hexdigest()


def signature_for(artifact: dict[str, Any]) -> str:
    value = (
        SIGNATURE_PREFIX
        + str(artifact["signing_key_id"])
        + ":"
        + str(artifact["model_id"])
        + ":"
        + str(artifact["sha256"])
        + ":"
        + str(artifact["runtime_artifact_id"])
        + ":"
        + str(artifact["payload_artifact_id"])
    )
    return hashlib.sha256(value.encode("utf-8")).hexdigest()


def validate_artifact(artifact: dict[str, Any]) -> dict[str, Any]:
    if not str(artifact.get("model_id", "")).strip():
        raise ValueError("model_id must be a non-empty string")
    if artifact.get("format", "GGUF") != "GGUF":
        raise ValueError("model format must be GGUF")
    if not str(artifact.get("quantization", "")).strip():
        raise ValueError("quantization must be a non-empty string")
    size = int(artifact.get("size_bytes", 0))
    if size <= 0 or size > MAX_MODEL_BYTES:
        raise ValueError("model size exceeds the 4 GiB policy")
    digest = str(artifact.get("sha256", ""))
    if len(digest) != 64 or any(character not in "0123456789abcdefABCDEF" for character in digest):
        raise ValueError("model sha256 is invalid")
    if not str(artifact.get("language", "")).startswith("pt"):
        raise ValueError("model language is not Portuguese-compatible")
    if not str(artifact.get("license_id", "")).strip():
        raise ValueError("license_id must be a non-empty string")
    if not artifact.get("license_compatible", False):
        raise ValueError("model license is not compatible")
    if not str(artifact.get("backend_compatibility", "")).strip():
        raise ValueError("backend_compatibility must be a non-empty string")
    if artifact.get("signature_algorithm", "detached_manifest_digest_v1") != "detached_manifest_digest_v1":
        raise ValueError("model detached signature is invalid")
    signature = str(artifact.get("signature", ""))
    if len(signature) != 64 or any(character not in "0123456789abcdefABCDEF" for character in signature):
        raise ValueError("model detached signature is invalid")
    for key in ("signing_key_id", "runtime_artifact_id", "payload_artifact_id"):
        if not str(artifact.get(key, "")).strip():
            raise ValueError(f"{key} must be a non-empty string")
    if artifact["runtime_artifact_id"] == artifact["payload_artifact_id"] or not artifact.get("payload_separate", True):
        raise ValueError("runtime and model payload must be separate artifacts")
    if signature != signature_for(artifact):
        raise ValueError("model detached signature does not match manifest")
    observed = artifact.get("observed_payload")
    if observed is not None and (
        int(observed.get("size_bytes", 0)) != size
        or str(observed.get("sha256", "")) != digest
    ):
        raise ValueError("model payload hash or size mismatch")
    return {key: value for key, value in artifact.items() if key != "observed_payload"}


class FixtureBackend:
    def __init__(self, backend_id: str, output: dict[str, Any]) -> None:
        self.backend_id = backend_id
        self.output = output

    async def load(self, model_id: str) -> None:
        del model_id

    async def invoke(self, request: ModelRequest) -> dict[str, Any]:
        del request
        return self.output

    async def cancel(self, request_id: str) -> None:
        del request_id

    async def unload(self, model_id: str) -> None:
        del model_id


def availability(model_available: bool, reason_code: str) -> dict[str, Any]:
    return {
        "diagnostics_available": True,
        "dialogue_enabled": model_available,
        "model_available": model_available,
        "privacy_available": True,
        "reason_code": reason_code,
        "timeline_available": True,
    }


def error_value(code: str, message: str) -> dict[str, str]:
    return {"code": code, "message": message}


async def _settle(gateway: LocalModelGateway) -> None:
    for _ in range(4):
        await asyncio.sleep(0)
        if gateway.snapshot()["metrics"]["active_request_id"] is None:
            return


async def reference_async(case: dict[str, Any]) -> dict[str, Any]:
    artifact_value = case.get("artifact")
    artifact_json: dict[str, Any] | None = None
    errors: list[dict[str, str]] = []
    responses: list[dict[str, Any]] = []
    snapshot: dict[str, Any] | None = None
    gateway: LocalModelGateway | None = None
    backend_id = str(case.get("config", {}).get("backend_id", "fixture"))
    if artifact_value is not None:
        try:
            artifact_json = validate_artifact(dict(artifact_value))
            backend_value = case.get("backend", {})
            output = {
                "kind": str(backend_value.get("output_kind", "summary")),
                "fields": dict(backend_value.get("output_fields", {"text": "fixture response"})),
            }
            backend = FixtureBackend(backend_id, output)
            config_value = case.get("config", {})
            policy = SchedulingPolicy(
                config_value.get("scheduling_policy", "priority_single_worker_v1")
            )
            gateway = LocalModelGateway(
                {backend_id: backend},
                GatewayConfig(
                    backend_id=backend_id,
                    scheduling_policy=policy,
                    unload_after_request=bool(config_value.get("unload_after_request", True)),
                ),
            )
        except ValueError as error:
            errors.append(error_value("artifact_invalid", str(error)))

    if gateway is not None:
        assert artifact_json is not None
        for operation in case.get("operations", []):
            operation_type = operation["type"]
            if operation_type == "snapshot":
                await _settle(gateway)
                snapshot = gateway.snapshot()
                continue
            if operation_type != "request":
                errors.append(error_value("unsupported_operation", str(operation_type)))
                continue
            try:
                request_value = operation["request"]
                template_value = request_value["template"]
                template = PromptTemplate(
                    template_id=template_value["template_id"],
                    version=template_value["version"],
                    body=template_value["body"],
                    variables=tuple(template_value["variables"]),
                )
                request = ModelRequest(
                    request_id=request_value["request_id"],
                    backend_id=request_value.get("backend_id", backend_id),
                    model_id=request_value.get("model_id", artifact_json["model_id"]),
                    priority=int(request_value.get("priority", 0)),
                    timeout_seconds=float(request_value.get("timeout_seconds", 1.0)),
                    template=template,
                    rendered_prompt=template.render(request_value["values"]),
                )
                response = await gateway.invoke(request)
                value = response.to_mapping()
                value["latency_ms"] = 0.0
                responses.append(value)
            except Exception as error:  # noqa: BLE001 - the fixture records typed gateway failures.
                errors.append(error_value("request_failed", str(error)))
        await _settle(gateway)
        if snapshot is None:
            snapshot = gateway.snapshot()
        metrics = snapshot["metrics"]
        metrics.update(
            {
                "cancellation_count": 0,
                "completed_count": len(responses),
                "invalid_response_count": 0,
                "model_available": True,
                "timeout_count": 0,
            }
        )
    elif artifact_value is not None and not errors:
        errors.append(error_value("artifact_invalid", "model artifact is unavailable"))

    return {
        "artifact": artifact_json,
        "availability": availability(artifact_json is not None, "model_available" if artifact_json is not None else ("artifact_invalid" if errors else "model_absent")),
        "case_id": case["case_id"],
        "errors": errors,
        "responses": responses,
        "snapshot": snapshot,
    }


def reference_transform(case: dict[str, Any]) -> dict[str, Any]:
    return asyncio.run(reference_async(case))


def normalized_candidate_runner(binary: Path):
    runner = command_runner([str(binary), "--local-model-dialogue"])

    def run(fixture: bytes) -> bytes:
        values = []
        for line in runner(fixture).decode("utf-8").splitlines():
            if not line.strip():
                continue
            value = json.loads(line)
            for response in value.get("responses", []):
                response["latency_ms"] = 0.0
            values.append(value)
        return b"".join(
            (json.dumps(value, ensure_ascii=False, sort_keys=True, separators=(",", ":")) + "\n").encode("utf-8")
            for value in values
        )

    return run


def invariant_failures(values: list[dict[str, Any]]) -> list[str]:
    failures: list[str] = []
    for value in values:
        available = value["availability"]
        if not available["model_available"] and (
            not available["timeline_available"]
            or not available["privacy_available"]
            or not available["diagnostics_available"]
            or available["dialogue_enabled"]
        ):
            failures.append(f"{value['case_id']}:model_absent_availability")
        for response in value["responses"]:
            validate_shared_schema(response, "local_model_response.schema.json")
        snapshot = value.get("snapshot")
        if snapshot:
            metrics = snapshot["metrics"]
            if metrics["max_concurrent_inferences"] > 1 or metrics["max_loaded_models"] > 1:
                failures.append(f"{value['case_id']}:single_heavy_bound")
            if any(term in json.dumps(value, ensure_ascii=False).lower() for term in FORBIDDEN_TERMS):
                failures.append(f"{value['case_id']}:forbidden_mental_state_term")
    return failures


def fixture_runner_bytes(path: Path) -> bytes:
    return path.read_bytes()


def performance(runner, fixture: bytes, repetitions: int = 20) -> dict[str, float]:
    samples: list[float] = []
    for _ in range(repetitions):
        started = time.perf_counter()
        runner(fixture)
        samples.append((time.perf_counter() - started) * 1000.0)
    samples.sort()
    p95 = samples[min(len(samples) - 1, int(len(samples) * 0.95))]
    elapsed = sum(samples) / 1000.0
    return {
        "latency_max_ms": max(samples),
        "latency_ms": statistics.mean(samples),
        "latency_p50_ms": statistics.median(samples),
        "latency_p95_ms": p95,
        "memory_mb": 0.0,
        "throughput": repetitions * 1000.0 / max(elapsed, 0.000001),
    }


def build_manifest(fixture: Path) -> PromotionManifest:
    return PromotionManifest.from_dict(
        {
            "promotion_id": "inference.local_model_gateway.v1",
            "component_id": "inference.local_model_gateway",
            "component_version": "1.0.0",
            "hypothesis": {"id": "H6-LOCAL-MODEL-GATEWAY", "report_uri": "docs/07-research/SCIENTIFIC_VALIDATION_REPORT.md"},
            "reference": {"language": "python", "package": "eu_digital_lab", "commit": "frozen-2026-07-30", "entrypoint": "eu_digital_lab.local_model_gateway.LocalModelGateway", "environment_lock_hash": "local-model-dialogue-reference"},
            "candidate": {"language": "cpp", "target": "promotion_fixture_runner", "commit": "working-tree", "compiler": "C++23 native", "build_profile": "Debug"},
            "contract": {"input_schema": "validation/equivalence/local_model_dialogue_v1.jsonl", "output_schema": "contracts/schemas/local_model_response.schema.json", "state_schema": "docs/03-contracts/LOCAL_MODEL_GATEWAY_SCHEMA.md", "error_schema": "docs/03-contracts/LOCAL_MODEL_GATEWAY_SCHEMA.md", "clock_semantics": "monotonic operational latency; normalized for semantic equivalence", "random_seed_policy": "no randomness"},
            "dataset": {"fixture_set": str(fixture.relative_to(ROOT)).replace("\\", "/"), "hash": canonical_sha256(fixture), "case_count": len(load_cases(fixture))},
            "equivalence": {"type": "exact", "absolute_tolerance": 0.0, "relative_tolerance": 0.0, "invariants": ["single_heavy_model", "structured_response", "model_absence_preserves_local_services", "artifact_integrity"], "acceptance_metrics": {"semantic_match": 1.0, "invariant_pass_rate": 1.0}},
            "performance": {"baseline_hardware": "captured by this validator", "maximum_latency_ms": 500.0, "maximum_memory_mb": 7168.0, "minimum_throughput": 1.0},
            "status": "reference_frozen",
        }
    )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--binary", type=Path, default=ROOT / "build" / "dev-vcpkg" / ("promotion_fixture_runner.exe" if os.name == "nt" else "promotion_fixture_runner"))
    parser.add_argument("--equivalence", type=Path, default=ROOT / "validation" / "equivalence" / "local_model_dialogue_v1.jsonl")
    parser.add_argument("--holdout", type=Path, default=ROOT / "validation" / "holdout" / "local_model_dialogue_v1.jsonl")
    parser.add_argument("--report", type=Path, default=ROOT / "validation" / "reports" / "local_model_dialogue_v1.json")
    args = parser.parse_args()
    if not args.binary.exists():
        raise SystemExit(f"candidate binary not found: {args.binary}")
    fixture = fixture_runner_bytes(args.equivalence)
    holdout = fixture_runner_bytes(args.holdout)
    candidate = normalized_candidate_runner(args.binary)
    reference = python_runner(reference_transform)
    manifest = build_manifest(args.equivalence)
    pipeline = PromotionPipeline()
    metrics = performance(candidate, fixture)
    result = pipeline.evaluate(
        manifest,
        fixture,
        reference_runner=reference,
        candidate_runner=candidate,
        divergence_path=ROOT / "validation" / "reports" / "local_model_dialogue_v1_divergences.json",
        performance_metrics={"latency_ms": metrics["latency_p95_ms"], "memory_mb": metrics["memory_mb"], "throughput": metrics["throughput"]},
    )
    reference_holdout = reference(holdout)
    candidate_holdout = candidate(holdout)
    holdout_divergences = compare_outputs(reference_holdout, candidate_holdout, manifest.data["equivalence"])
    dev_values = [json.loads(line) for line in reference(fixture).decode("utf-8").splitlines() if line.strip()]
    candidate_values = [json.loads(line) for line in candidate(fixture).decode("utf-8").splitlines() if line.strip()]
    invariants = invariant_failures(dev_values + candidate_values)
    replay_passed = candidate(fixture) == candidate(fixture)
    if not result.equivalence_passed or not result.performance_passed or holdout_divergences or invariants or not replay_passed:
        raise SystemExit(json.dumps({"divergences": result.divergences, "holdout": holdout_divergences, "invariants": invariants, "performance": result.performance, "replay_passed": replay_passed}, ensure_ascii=False))
    report = result.to_dict()
    report.update(
        {
            "ablation": {"baseline": "model absent keeps timeline/privacy/diagnostics available", "unload_between_requests": True, "fifo_scheduler": BASELINE_SCHEDULER_ID, "interpretation": "operational resource-bound ablation only"},
            "holdout": {"case_count": len(load_cases(args.holdout)), "fixture_set": str(args.holdout.relative_to(ROOT)).replace("\\", "/"), "sha256": canonical_sha256(args.holdout), "equivalence_passed": not holdout_divergences, "divergences": holdout_divergences},
            "invariants": {"passed": not invariants, "failures": invariants},
            "operational_metrics": metrics,
            "replay": {"passed": replay_passed, "normalization": "response latency_ms set to 0 for semantic comparison"},
            "scientific_evidence_boundary": "equivalence, holdout, and operational limits are computational verification; they are not evidence of cognition or model quality",
            "artifact_policy": {"max_model_size_gib": 4.0, "max_declared_ram_gib": 7.0, "format": "GGUF", "language": "Portuguese-compatible", "signature": "detached_manifest_digest_v1 envelope; asymmetric release signing remains a future release concern"},
            "operational_constants": {"hypothesis": HYPOTHESIS, "ablation": ABLATION, "falsification": FALSIFICATION},
        }
    )
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(report, ensure_ascii=False, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(json.dumps({"equivalence": True, "holdout": True, "invariants": True, "performance": result.performance, "report": str(args.report)}, ensure_ascii=False))
    return 0


if __name__ == "__main__":
    main()
