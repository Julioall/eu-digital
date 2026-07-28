import json
import sys
import tempfile
import unittest
from pathlib import Path

REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
LAB_ROOT = REPOSITORY_ROOT / "python"
if str(LAB_ROOT) not in sys.path:
    sys.path.insert(0, str(LAB_ROOT))

from eu_digital_lab.promotion import (
    PromotionGateError,
    PromotionManifest,
    PromotionPipeline,
    PromotionRegistry,
    python_runner,
    write_fixture_set,
)


def manifest() -> PromotionManifest:
    return PromotionManifest.from_dict(
        {
            "promotion_id": "promotion.test.v1",
            "component_id": "test.component",
            "component_version": "1.0.0",
            "hypothesis": {"id": "H-TEST", "report_uri": "reports/H-TEST.json"},
            "reference": {
                "language": "python",
                "package": "eu_digital_lab.test",
                "commit": "python-commit",
                "entrypoint": "test.reference",
                "environment_lock_hash": "env-hash",
            },
            "candidate": {
                "language": "cpp",
                "target": "promotion_fixture_runner",
                "commit": "cpp-commit",
                "compiler": "gcc-13",
                "build_profile": "Debug",
            },
            "contract": {
                "input_schema": "contracts/input.json",
                "output_schema": "contracts/output.json",
                "state_schema": None,
                "error_schema": "contracts/error.json",
                "clock_semantics": "virtual-ms",
                "random_seed_policy": "seed-recorded",
            },
            "dataset": {
                "fixture_set": "fixtures/test.jsonl",
                "hash": "",
                "case_count": 1,
            },
            "equivalence": {
                "type": "exact",
                "absolute_tolerance": None,
                "relative_tolerance": None,
                "invariants": ["output-is-json"],
                "acceptance_metrics": {"exact_match": 1.0},
            },
            "performance": {
                "baseline_hardware": "local-test",
                "maximum_latency_ms": 10.0,
                "maximum_memory_mb": 10.0,
                "minimum_throughput": 1.0,
            },
            "status": "draft",
        }
    )


class PromotionTests(unittest.TestCase):
    def test_fixture_generator_and_python_runner_are_canonical(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "fixture.jsonl"
            fixture_bytes = write_fixture_set(path, [{"input": 1, "case_id": "one"}])
            self.assertEqual(fixture_bytes, b'{"case_id":"one","input":1}\n')
            self.assertEqual(
                python_runner(lambda case: case)(fixture_bytes), fixture_bytes
            )

    def test_reference_freeze_and_tolerance_review(self) -> None:
        current = manifest().freeze_reference(b'{"value":1}\n', commit="frozen-commit")
        self.assertEqual(current.data["status"], "reference_frozen")
        self.assertEqual(current.data["reference"]["commit"], "frozen-commit")
        self.assertTrue(current.data["dataset"]["hash"])
        with self.assertRaises(ValueError):
            current.change_tolerance(absolute=0.1)
        reviewed = current.change_tolerance(
            absolute=0.1, justification="validated numeric noise", review_id="review-2"
        )
        self.assertEqual(reviewed.data["equivalence"]["absolute_tolerance"], 0.1)
        self.assertEqual(
            reviewed.data["equivalence"]["tolerance_review_id"], "review-2"
        )

    def test_pipeline_delivers_identical_input_and_persists_empty_divergences(
        self,
    ) -> None:
        fixture_bytes = b'{"case_id":"one","input":1}\n'
        observed: list[bytes] = []

        def observe(value: bytes) -> bytes:
            observed.append(value)
            return value

        with tempfile.TemporaryDirectory() as directory:
            divergence_path = Path(directory) / "divergences.json"
            result = PromotionPipeline().evaluate(
                manifest().freeze_reference(fixture_bytes, commit="frozen"),
                fixture_bytes,
                reference_runner=observe,
                candidate_runner=observe,
                divergence_path=divergence_path,
                performance_metrics={
                    "latency_ms": 1.0,
                    "memory_mb": 1.0,
                    "throughput": 2.0,
                },
            )
            self.assertEqual(observed, [fixture_bytes, fixture_bytes])
            self.assertTrue(result.equivalence_passed)
            self.assertEqual(result.divergences, [])
            self.assertEqual(
                json.loads(divergence_path.read_text(encoding="utf-8")), []
            )
            self.assertEqual(result.input_sha256["python"], result.input_sha256["cpp"])

    def test_pipeline_persists_semantic_divergence(self) -> None:
        fixture_bytes = b'{"case_id":"one","input":1}\n'
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "divergences.json"
            result = PromotionPipeline().evaluate(
                manifest().freeze_reference(fixture_bytes, commit="frozen"),
                fixture_bytes,
                reference_runner=lambda value: b'{"output":1}\n',
                candidate_runner=lambda value: b'{"output":2}\n',
                divergence_path=path,
            )
            self.assertFalse(result.equivalence_passed)
            self.assertEqual(
                result.divergences[0]["classification"], "semantic-mismatch"
            )
            self.assertEqual(len(json.loads(path.read_text(encoding="utf-8"))), 1)

    def test_runner_failure_is_persisted_as_divergence(self) -> None:
        fixture_bytes = b'{"case_id":"one","input":1}\n'

        def failing_runner(value: bytes) -> bytes:
            raise RuntimeError("candidate crashed")

        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "divergences.json"
            result = PromotionPipeline().evaluate(
                manifest().freeze_reference(fixture_bytes, commit="frozen"),
                fixture_bytes,
                reference_runner=lambda value: value,
                candidate_runner=failing_runner,
                divergence_path=path,
            )
            self.assertFalse(result.equivalence_passed)
            self.assertEqual(result.divergences[0]["classification"], "runner-failure")
            self.assertEqual(result.divergences[0]["runner"], "cpp")
            self.assertEqual(len(json.loads(path.read_text(encoding="utf-8"))), 1)

    def test_numeric_tolerance_is_applied_without_claiming_exact_equality(self) -> None:
        fixture_bytes = b'{"case_id":"one","input":1}\n'
        numeric_manifest = json.loads(json.dumps(manifest().data))
        numeric_manifest["equivalence"]["type"] = "numeric"
        current = PromotionManifest.from_dict(numeric_manifest).freeze_reference(
            fixture_bytes, commit="frozen"
        )
        current = current.change_tolerance(
            absolute=0.01, justification="measurement noise", review_id="review-1"
        )
        result = PromotionPipeline().evaluate(
            current,
            fixture_bytes,
            reference_runner=lambda value: b'{"output":1.0}\n',
            candidate_runner=lambda value: b'{"output":1.005}\n',
        )
        self.assertTrue(result.equivalence_passed)

    def test_registry_rejects_unapproved_components(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "registry.json"
            registry = PromotionRegistry(path)
            with self.assertRaises(PromotionGateError):
                registry.require("test.component")
            registry.approve(manifest().with_status("promoted", review_id="review-1"))
            selected = registry.require("test.component")
            self.assertEqual(selected["promotion_id"], "promotion.test.v1")

    def test_report_contains_scientific_and_operational_provenance(self) -> None:
        fixture_bytes = b'{"case_id":"one","input":1}\n'
        current = manifest().freeze_reference(fixture_bytes, commit="frozen")
        result = PromotionPipeline().evaluate(
            current,
            fixture_bytes,
            reference_runner=lambda value: value,
            candidate_runner=lambda value: value,
            performance_metrics={
                "latency_ms": 1.0,
                "memory_mb": 1.0,
                "throughput": 2.0,
            },
        )
        report = result.to_dict()
        self.assertEqual(report["hypothesis"]["id"], "H-TEST")
        self.assertEqual(report["commits"]["python"], "frozen")
        self.assertIn("hardware", report)
        self.assertIn("performance", report)
        self.assertIn("dataset", report)


if __name__ == "__main__":
    unittest.main()
