import json
import sys
import tempfile
import unittest
from pathlib import Path

REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
LAB_ROOT = REPOSITORY_ROOT / "python"
if str(LAB_ROOT) not in sys.path:
    sys.path.insert(0, str(LAB_ROOT))

from eu_digital_lab.evaluation import (
    DatasetRepository,
    ExperimentConfig,
    ExperimentRunner,
    FaultInjector,
    HoldoutAccessError,
    ReplayLog,
    VirtualClock,
    run_metamorphic_test,
)


class EvaluationHarnessTests(unittest.TestCase):
    def test_feature_flag_disables_module_without_code_change(self) -> None:
        def baseline(value: int, context: object) -> int:
            return value

        def treatment(value: int, context: object) -> int:
            return value + (1 if context.module_enabled("novel-module") else 0)  # type: ignore[attr-defined]

        config = ExperimentConfig(
            experiment_id="ablation",
            hypothesis="the module changes the score",
            feature_flags={"novel-module": False},
            repo_root=REPOSITORY_ROOT,
        )
        report = ExperimentRunner().run(
            config, 10, baseline=baseline, treatment=treatment
        )
        self.assertEqual(report.trials["treatment"].output, 10)
        self.assertFalse(report.configuration["feature_flags"]["novel-module"])

    def test_report_records_provenance_and_compares_metrics(self) -> None:
        config = ExperimentConfig(
            experiment_id="comparison",
            hypothesis="treatment improves the score",
            backend="reference-python",
            seed=11,
            configuration={"threshold": 0.5},
            repo_root=REPOSITORY_ROOT,
        )
        report = ExperimentRunner().run(
            config,
            1,
            baseline=lambda value, context: value,
            treatment=lambda value, context: value + 1,
            cognitive_metrics=lambda output, truth: {
                "score": [float(output), float(output) + 0.1]
            },
            operational_metrics=lambda output: {
                "latency_ms": 1.0 if output == 1 else 2.0
            },
            ground_truth=2,
            ground_truth_metrics=lambda output, truth: {
                "exact": float(output == truth)
            },
        )
        self.assertEqual(report.provenance["backend"], "reference-python")
        self.assertEqual(report.configuration["seed"], 11)
        self.assertIn("score", report.comparisons["cognitive"])
        self.assertIn("exact", report.trials["treatment"].ground_truth)
        self.assertEqual(report.trials["treatment"].ground_truth["exact"].mean, 1.0)
        self.assertIn("latency_ms", report.comparisons["operational"])

    def test_holdout_is_hashed_and_locked_by_default(self) -> None:
        repository = DatasetRepository(
            REPOSITORY_ROOT / "datasets" / "synthetic" / "v1"
        )
        metadata = repository.metadata("test")
        self.assertTrue(metadata["locked"])
        self.assertTrue(all(len(item["sha256"]) == 64 for item in metadata["files"]))
        with self.assertRaises(HoldoutAccessError):
            repository.load("test")
        loaded = repository.load("test", purpose="final-evaluation")
        self.assertEqual(len(loaded), 1)
        self.assertEqual(repository.access_log[0]["purpose"], "final-evaluation")

    def test_metamorphic_check_detects_known_mutation(self) -> None:
        result = run_metamorphic_test(
            3,
            transform=lambda value: value + 1,
            system=lambda value: value + 1,
            relation=lambda original, transformed: original == transformed,
            name="translation-invariance",
        )
        self.assertTrue(result.violation_detected)
        self.assertFalse(result.passed)

    def test_virtual_clock_and_replay_are_deterministic(self) -> None:
        clock = VirtualClock()
        replay = ReplayLog()
        replay.record({"event": "first"}, at_ms=10)
        replay.record({"event": "second"}, at_ms=25)
        received: list[tuple[int, str]] = []
        replay.replay(
            lambda event: received.append((clock.now_ms, event["event"])), clock
        )
        self.assertEqual(received, [(10, "first"), (25, "second")])

    def test_fault_injection_is_explicit_and_local(self) -> None:
        faults = FaultInjector({"storage": 1})
        with self.assertRaises(RuntimeError):
            faults.check("storage")
        faults.check("storage")
        self.assertEqual(faults.consumed, ("storage",))

    def test_report_can_be_written_as_json(self) -> None:
        config = ExperimentConfig(experiment_id="serializable", hypothesis="round trip")
        report = ExperimentRunner().run(
            config,
            {"value": 1},
            baseline=lambda value, context: value,
            treatment=lambda value, context: value,
        )
        with tempfile.TemporaryDirectory() as directory:
            target = Path(directory) / "report.json"
            report.write(target)
            saved = json.loads(target.read_text(encoding="utf-8"))
        self.assertEqual(saved["experiment_id"], "serializable")
        self.assertEqual(saved["trials"]["baseline"]["status"], "completed")


if __name__ == "__main__":
    unittest.main()
