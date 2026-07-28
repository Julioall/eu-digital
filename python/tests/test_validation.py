import json
import sys
import tempfile
import unittest
from collections.abc import Mapping
from pathlib import Path
from typing import Any

REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
LAB_ROOT = REPOSITORY_ROOT / "python"
if str(LAB_ROOT) not in sys.path:
    sys.path.insert(0, str(LAB_ROOT))

from eu_digital_lab.evaluation import ReplayLog
from eu_digital_lab.validation import (
    GateEvidence,
    JitterSchedule,
    ValidationGateError,
    ValidationGateRunner,
    ValidationProtocol,
    audit_export,
    build_longitudinal_report,
    compare_backends,
    run_failure_scenario,
)


def protocol() -> ValidationProtocol:
    return ValidationProtocol(
        protocol_id="protocol.test.v1",
        hypothesis_id="H-TEST",
        metrics=("accuracy", "latency_ms"),
        acceptance_criteria={"accuracy": 0.8},
        review_id="independent-review-1",
    )


def passing_gates() -> dict[str, GateEvidence]:
    return {
        "equivalence": GateEvidence("equivalence", True, {"exact": True}),
        "ground_truth": GateEvidence("ground_truth", True, {"known_truth": True}),
        "holdout": GateEvidence(
            "holdout", True, {"sha256": "a" * 64, "access_registered": True}
        ),
        "metamorphic": GateEvidence("metamorphic", True, {"mutation_detected": True}),
        "replay": GateEvidence(
            "replay", True, {"clock_controlled": True, "ordered": True}
        ),
        "faults": GateEvidence("faults", True, {"reproducible": True}),
        "export": GateEvidence(
            "export",
            True,
            {
                "hashes": {"original": "a" * 64, "exported": "b" * 64},
                "quantization": "int8",
            },
        ),
        "online": GateEvidence(
            "online", True, {"replay_completed": True, "online_completed": True}
        ),
    }


class ValidationTests(unittest.TestCase):
    def test_protocol_freezes_and_cannot_be_changed_silently(self) -> None:
        frozen = protocol().freeze()
        self.assertTrue(frozen.frozen)
        with self.assertRaises(ValidationGateError):
            frozen.revise(metrics=("accuracy",))

    def test_equivalence_alone_never_approves_validation(self) -> None:
        gates = {"equivalence": GateEvidence("equivalence", True, {"exact": True})}
        report = ValidationGateRunner().evaluate(protocol().freeze(), gates=gates)
        self.assertFalse(report.overall_passed)
        self.assertFalse(report.equivalence_is_sufficient)
        self.assertFalse(report.gates["ground_truth"].passed)

    def test_all_independent_gates_are_required(self) -> None:
        report = ValidationGateRunner().evaluate(
            protocol().freeze(),
            gates=passing_gates(),
            cognitive_metrics={"accuracy": 0.9},
            operational_metrics={"latency_ms": 1.2},
            backend_comparison={"passed": True, "hardware": {"cpp": "local"}},
        )
        self.assertTrue(report.overall_passed)
        self.assertEqual(report.cognitive_metrics["accuracy"], 0.9)
        self.assertEqual(report.operational_metrics["latency_ms"], 1.2)
        self.assertTrue(report.backend_comparison["passed"])

    def test_claimed_pass_without_required_evidence_is_rejected(self) -> None:
        gates = passing_gates()
        gates["holdout"] = GateEvidence("holdout", True, {})
        report = ValidationGateRunner().evaluate(protocol().freeze(), gates=gates)
        self.assertFalse(report.gates["holdout"].passed)
        self.assertFalse(report.overall_passed)

    def test_holdout_and_replay_evidence_are_explicit(self) -> None:
        gates = passing_gates()
        gates["holdout"] = GateEvidence(
            "holdout", True, {"sha256": "a" * 64, "access_registered": True}
        )
        gates["replay"] = GateEvidence(
            "replay", True, {"clock_controlled": True, "ordered": True}
        )
        report = ValidationGateRunner().evaluate(protocol().freeze(), gates=gates)
        self.assertTrue(report.gates["holdout"].evidence["access_registered"])
        self.assertTrue(report.gates["replay"].evidence["clock_controlled"])

    def test_jitter_and_failure_scenario_are_reproducible(self) -> None:
        first = JitterSchedule.deterministic(count=4, seed=17, maximum_ms=5)
        second = JitterSchedule.deterministic(count=4, seed=17, maximum_ms=5)
        self.assertEqual(first.delays_ms, second.delays_ms)

        def operation(faults: object) -> None:
            faults.check("sensor")  # type: ignore[attr-defined]

        first_failure = run_failure_scenario("sensor-loss", {"sensor": 1}, operation)
        second_failure = run_failure_scenario("sensor-loss", {"sensor": 1}, operation)
        self.assertEqual(first_failure, second_failure)
        self.assertEqual(first_failure["status"], "failed")

    def test_export_audit_reports_quantization_and_differences(self) -> None:
        result = audit_export(
            b'{"score":1.0}\n',
            b'{"score":0.99}\n',
            quantization="int8",
            accuracy_before=0.90,
            accuracy_after=0.89,
            accuracy_tolerance=0.02,
        )
        self.assertTrue(result["passed"])
        self.assertEqual(result["quantization"], "int8")
        self.assertEqual(len(result["hashes"]["original"]), 64)
        self.assertNotEqual(result["hashes"]["original"], result["hashes"]["exported"])

    def test_backend_and_hardware_comparison_is_recorded(self) -> None:
        result = compare_backends(
            {"python-cpu": {"accuracy": 0.9}, "cpp-cpu": {"accuracy": 0.9}},
            {"python-cpu": "local-python", "cpp-cpu": "local-cpp"},
        )
        self.assertTrue(result["passed"])
        self.assertEqual(result["hardware"]["cpp-cpu"], "local-cpp")
        self.assertEqual(result["metrics"]["accuracy"]["difference"], 0.0)

    def test_online_session_requires_successful_replay(self) -> None:
        replay = ReplayLog()
        replay.record({"event": "replay"}, at_ms=10)
        events: list[str] = []

        def replay_handler(event: Mapping[str, Any]) -> None:
            events.append(str(event["event"]))

        with self.assertRaises(ValidationGateError):
            ValidationGateRunner.run_online_after_replay(
                replay,
                replay_handler,
                [{"event": "online"}],
                lambda event, clock: events.append(event["event"]),
                False,
            )
        result = ValidationGateRunner.run_online_after_replay(
            replay,
            replay_handler,
            [{"event": "online"}],
            lambda event, clock: events.append(event["event"]),
            True,
        )
        self.assertTrue(result["replay_completed"])
        self.assertEqual(events, ["replay", "online"])

    def test_longitudinal_report_separates_metrics_and_tracks_drift(self) -> None:
        report = build_longitudinal_report(
            [
                {
                    "session_id": "s1",
                    "cognitive_metrics": {"accuracy": 0.8},
                    "operational_metrics": {"latency_ms": 2.0},
                },
                {
                    "session_id": "s2",
                    "cognitive_metrics": {"accuracy": 0.9},
                    "operational_metrics": {"latency_ms": 3.0},
                },
            ]
        )
        self.assertEqual(report["cognitive"]["accuracy"]["mean"], 0.85)
        self.assertEqual(report["cognitive"]["accuracy"]["drift"], 0.1)
        self.assertEqual(report["operational"]["latency_ms"]["mean"], 2.5)
        self.assertNotIn("latency_ms", report["cognitive"])

    def test_validation_report_is_serializable(self) -> None:
        report = ValidationGateRunner().evaluate(
            protocol().freeze(), gates=passing_gates()
        )
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "validation.json"
            report.write(path)
            saved = json.loads(path.read_text(encoding="utf-8"))
        self.assertTrue(saved["overall_passed"])
        self.assertFalse(saved["equivalence_is_sufficient"])


if __name__ == "__main__":
    unittest.main()
