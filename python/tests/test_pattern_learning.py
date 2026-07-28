import sys
import unittest
from pathlib import Path

LAB_ROOT = Path(__file__).resolve().parents[1]
if str(LAB_ROOT) not in sys.path:
    sys.path.insert(0, str(LAB_ROOT))

from eu_digital_lab.pattern_learning import (
    PatternConfig,
    PatternLearner,
    PatternLearningError,
    PatternStatus,
)


class PatternLearningTests(unittest.TestCase):
    def test_support_threshold_controls_promotion(self) -> None:
        learner = PatternLearner(PatternConfig(min_support=2, distance_threshold=0.1), stream_id="stream-1")

        candidate = learner.observe({"x": 0.0, "y": 0.0}, "obs-1", "2026-01-01T00:00:00Z")
        promoted = learner.observe({"x": 0.0, "y": 0.0}, "obs-2", "2026-01-01T00:00:01Z")

        self.assertEqual(candidate.status, PatternStatus.candidate)
        self.assertEqual(promoted.status, PatternStatus.promoted)
        self.assertEqual(promoted.support, 2)
        self.assertEqual(promoted.observation_refs, ["obs-1", "obs-2"])

    def test_feedback_changes_confidence_without_erasing_provenance(self) -> None:
        learner = PatternLearner(PatternConfig(min_support=1), stream_id="stream-1")
        pattern = learner.observe({"x": 1.0}, "obs-1", "2026-01-01T00:00:00Z")
        before = pattern.confidence

        corrected = learner.feedback(pattern.pattern_id, positive=False, reference="human-1")

        self.assertLess(corrected.confidence, before)
        self.assertEqual(corrected.status, PatternStatus.candidate)
        self.assertEqual(corrected.feedback["negative"], 1)
        self.assertIn("obs-1", corrected.observation_refs)

    def test_concept_drift_creates_a_new_version(self) -> None:
        learner = PatternLearner(PatternConfig(min_support=2, distance_threshold=0.1), stream_id="stream-1")
        first = learner.observe({"x": 0.0}, "obs-1", "2026-01-01T00:00:00Z")
        learner.observe({"x": 0.0}, "obs-2", "2026-01-01T00:00:01Z")

        drifted = learner.observe({"x": 1.0}, "obs-3", "2026-01-02T00:00:00Z")

        self.assertNotEqual(drifted.pattern_id, first.pattern_id)
        self.assertEqual(drifted.version, 2)
        self.assertEqual(drifted.parent_pattern_id, first.pattern_id)
        self.assertEqual(drifted.drift_reason, "concept_drift")

    def test_metrics_and_ablation_are_registered(self) -> None:
        learner = PatternLearner(PatternConfig(min_support=2), stream_id="stream-1")
        learner.observe({"x": 0.0}, "obs-1", "2026-01-01T00:00:00Z")
        learner.observe({"x": 0.0}, "obs-2", "2026-01-01T00:00:01Z")

        metrics = learner.metrics()

        self.assertEqual(metrics["baseline_id"], "online_exact_threshold_v1")
        self.assertTrue(metrics["registered"])
        self.assertIn("support", metrics["clusters"][0])
        self.assertIn("ablation", metrics)
        self.assertIn("falsification", metrics)

    def test_same_stream_and_observations_are_deterministic(self) -> None:
        config = PatternConfig(min_support=2, distance_threshold=0.2)
        first = PatternLearner(config, stream_id="stream-1")
        second = PatternLearner(config, stream_id="stream-1")
        for learner in (first, second):
            learner.observe({"x": 0.0, "y": 1.0}, "obs-1", "2026-01-01T00:00:00Z")
            learner.observe({"x": 0.1, "y": 1.0}, "obs-2", "2026-01-01T00:00:01Z")

        self.assertEqual(first.snapshot(), second.snapshot())

    def test_invalid_feedback_and_features_are_typed_errors(self) -> None:
        learner = PatternLearner(stream_id="stream-1")
        with self.assertRaises(PatternLearningError):
            learner.observe({}, "obs-1", "2026-01-01T00:00:00Z")
        with self.assertRaises(PatternLearningError):
            learner.feedback("missing", positive=True, reference="human-1")


if __name__ == "__main__":
    unittest.main()
