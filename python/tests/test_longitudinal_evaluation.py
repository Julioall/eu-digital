import sys
import unittest
from pathlib import Path

LAB_ROOT = Path(__file__).resolve().parents[1]
if str(LAB_ROOT) not in sys.path:
    sys.path.insert(0, str(LAB_ROOT))

from eu_digital_lab.longitudinal_evaluation import (
    BASELINE_POLICY_ID,
    LongitudinalEvaluationError,
    LongitudinalEvaluator,
    LongitudinalProtocol,
)


class LongitudinalEvaluationTests(unittest.TestCase):
    def _protocol(self):
        return LongitudinalProtocol.freeze(
            protocol_id="study.protocol.v1",
            study_id="study-1",
            metrics=("retention_score", "calibration_ece", "accuracy"),
            acceptance_criteria={"retention_score": 0.7, "calibration_ece": 0.2},
            holdout_sha256="a" * 64,
        )

    def _evaluator(self) -> LongitudinalEvaluator:
        evaluator = LongitudinalEvaluator(self._protocol())
        for day, captured_at, retention, calibration, version, digest in (
            (7, "2026-01-07T00:00:00+00:00", 0.95, 0.10, 1, "self-a"),
            (30, "2026-01-30T00:00:00+00:00", 0.80, 0.15, 2, "self-b"),
            (90, "2026-03-31T00:00:00+00:00", 0.70, 0.22, 4, "self-c"),
        ):
            evaluator.record_snapshot(
                checkpoint_day=day,
                captured_at=captured_at,
                cognitive_metrics={
                    "retention_score": retention,
                    "calibration_ece": calibration,
                    "accuracy": 0.8 + day / 1000,
                },
                operational_metrics={"latency_ms": float(day)},
                self_model_version=version,
                self_model_digest=digest,
                source_refs=(f"session-{day}",),
            )
        return evaluator

    def test_protocol_and_snapshots_are_frozen_and_schema_backed(self) -> None:
        protocol = self._protocol()
        self.assertTrue(protocol.frozen)
        self.assertEqual(len(protocol.protocol_hash), 64)
        self.assertEqual(protocol.to_mapping()["holdout_sha256"], "a" * 64)
        snapshot = self._evaluator().snapshots()[0]
        self.assertEqual(snapshot.protocol_hash, protocol.protocol_hash)
        self.assertEqual(snapshot.to_mapping()["checkpoint_day"], 7)

    def test_report_replays_identically_from_snapshots(self) -> None:
        evaluator = self._evaluator()
        report = evaluator.report()
        replayed = LongitudinalEvaluator.replay(evaluator.protocol, evaluator.snapshots()).to_mapping()

        self.assertEqual(replayed, report.to_mapping())
        self.assertEqual(report.baseline_policy_id, BASELINE_POLICY_ID)
        self.assertEqual([point["checkpoint_day"] for point in report.retention_curve], [7, 30, 90])

    def test_gains_losses_and_calibration_are_reported_separately(self) -> None:
        report = self._evaluator().report().to_mapping()

        cognitive = report["change_report"]["cognitive"]
        operational = report["change_report"]["operational"]
        self.assertEqual(cognitive["retention_score"]["status"], "loss")
        self.assertEqual(cognitive["accuracy"]["status"], "gain")
        self.assertEqual(operational["latency_ms"]["status"], "gain")
        self.assertNotIn("latency_ms", report["change_report"]["cognitive"])
        self.assertEqual(report["calibration"]["observed_count"], 3)

    def test_self_model_drift_is_quantified_without_claiming_experience(self) -> None:
        drift = self._evaluator().report().self_model_drift

        self.assertEqual(drift["version_delta"], 3)
        self.assertEqual(drift["digest_change_count"], 2)
        self.assertTrue(drift["changed"])

    def test_frozen_checkpoints_and_invalid_protocols_are_rejected(self) -> None:
        evaluator = self._evaluator()
        with self.assertRaises(LongitudinalEvaluationError):
            evaluator.record_snapshot(
                checkpoint_day=30,
                captured_at="2026-01-31T00:00:00+00:00",
                cognitive_metrics={},
                operational_metrics={},
                self_model_version=3,
                self_model_digest="self-x",
                source_refs=("session-x",),
            )
        with self.assertRaises(LongitudinalEvaluationError):
            LongitudinalProtocol.freeze(
                protocol_id="invalid",
                study_id="study-1",
                metrics=("retention_score",),
                acceptance_criteria={},
                holdout_sha256="not-a-hash",
            )


if __name__ == "__main__":
    unittest.main()
