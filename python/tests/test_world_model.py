import sys
import unittest
from pathlib import Path

LAB_ROOT = Path(__file__).resolve().parents[1]
if str(LAB_ROOT) not in sys.path:
    sys.path.insert(0, str(LAB_ROOT))

from eu_digital_lab.global_workspace import (
    GlobalWorkspace,
    WorkspaceCandidate,
    WorkspaceConfig,
)
from eu_digital_lab.world_model import (
    FREQUENCY_BASELINE_ID,
    PREDICTOR_POLICY_ID,
    ModelPolicy,
    PredictionConfig,
    WorldModel,
    WorldModelError,
    prediction_error_to_salience,
)


class WorldModelTests(unittest.TestCase):
    def _seed(self, model: WorldModel, states: list[str]) -> None:
        for index, state in enumerate(states):
            model.observe(state, f"event-{index}", f"2026-01-01T00:00:{index:02d}Z")

    def test_prediction_distribution_and_error_contracts_are_versioned(self) -> None:
        model = WorldModel(stream_id="stream-1", policy=ModelPolicy.incremental)
        self._seed(model, ["idle", "active", "idle"])

        prediction = model.predict(("idle",), predicted_at="2026-01-01T00:01:00Z", candidate_states=("idle", "active"))
        self.assertAlmostEqual(sum(prediction.predicted_distribution.values()), 1.0)
        self.assertIsNone(prediction.log_loss)
        scored = model.score(prediction, "active", "2026-01-01T00:01:01Z")

        self.assertEqual(scored.model_id, PREDICTOR_POLICY_ID)
        self.assertIsNotNone(scored.log_loss)
        self.assertIn("log_loss", scored.error_mapping("2026-01-01T00:01:01Z"))
        self.assertEqual(scored.to_mapping()["schema_version"], "1.0")

    def test_incremental_markov_beats_frequency_on_sequential_holdout(self) -> None:
        treatment = WorldModel(stream_id="treatment", policy=ModelPolicy.incremental)
        baseline = WorldModel(stream_id="baseline", policy=ModelPolicy.frequency)
        self._seed(treatment, ["a", "b", "a", "b"])
        self._seed(baseline, ["a", "b", "a", "b"])

        treatment_losses: list[float] = []
        baseline_losses: list[float] = []
        for index, (context, state) in enumerate([(("a", "b"), "a"), (("b", "a"), "b")]):
            treatment_prediction = treatment.predict(context, predicted_at=f"2026-01-01T00:02:{index:02d}Z")
            baseline_prediction = baseline.predict(context, predicted_at=f"2026-01-01T00:02:{index:02d}Z")
            treatment_score = treatment.score(treatment_prediction, state, f"2026-01-01T00:02:{index:02d}Z")
            baseline_score = baseline.score(baseline_prediction, state, f"2026-01-01T00:02:{index:02d}Z")
            if treatment_score.log_loss is None or baseline_score.log_loss is None:
                raise AssertionError("score must contain log loss")
            treatment_losses.append(treatment_score.log_loss)
            baseline_losses.append(baseline_score.log_loss)
            treatment.observe(state, f"holdout-treatment-{index}", f"2026-01-01T00:03:{index:02d}Z")
            baseline.observe(state, f"holdout-baseline-{index}", f"2026-01-01T00:03:{index:02d}Z")

        self.assertLess(sum(treatment_losses) / len(treatment_losses), sum(baseline_losses) / len(baseline_losses))
        self.assertEqual(baseline.model_id, FREQUENCY_BASELINE_ID)

    def test_prediction_error_increases_auditable_workspace_surprise(self) -> None:
        model = WorldModel(stream_id="stream-1", policy=ModelPolicy.incremental)
        self._seed(model, ["a", "a", "a", "b"])
        prediction = model.predict(("a",), predicted_at="2026-01-01T00:01:00Z", candidate_states=("a", "b"))
        scored = model.score(prediction, "b", "2026-01-01T00:01:01Z")
        self.assertGreater(scored.salience_contribution, 0.0)
        if scored.log_loss is None:
            raise AssertionError("score must contain log loss")
        self.assertEqual(prediction_error_to_salience(scored.log_loss), scored.salience_contribution)

        workspace = GlobalWorkspace("workspace-1", "session-1", WorkspaceConfig(capacity=1))
        candidate = WorkspaceCandidate(
            candidate_id="prediction-1",
            session_id="session-1",
            source_kind="internal",
            source_refs=(scored.prediction_id,),
            observed_at="2026-01-01T00:01:01Z",
            content=scored.to_mapping(),
            salience_signals={"surprise": scored.salience_contribution},
        )
        snapshot = workspace.admit(candidate, now="2026-01-01T00:01:02Z")
        self.assertEqual(snapshot.active_items[0].salience.observed_factors["surprise"], scored.salience_contribution)
        self.assertIn("surprise", snapshot.active_items[0].salience.observed_factors)

    def test_drift_reduces_confidence_and_starts_relearning(self) -> None:
        config = PredictionConfig(drift_window=2, drift_threshold=0.1)
        model = WorldModel(stream_id="stream-1", policy=ModelPolicy.incremental, config=config)
        self._seed(model, ["a", "a", "a"])
        for index in range(2):
            prediction = model.predict(("a",), predicted_at=f"2026-01-01T00:02:{index:02d}Z", candidate_states=("a", "b"))
            scored = model.score(prediction, "b", f"2026-01-01T00:02:{index:02d}Z")
            if index == 0:
                self.assertEqual(scored.drift_id, None)

        drift = model.latest_drift()
        self.assertIsNotNone(drift)
        assert drift is not None
        self.assertLess(drift.confidence_after, drift.confidence_before)
        self.assertTrue(drift.relearning_started)
        self.assertEqual(model.metrics()["relearning_started"], True)
        model.observe("b", "relearn-1", "2026-01-01T00:03:00Z")
        self.assertEqual(model.metrics()["relearning_observations"], 1)

    def test_invalid_inputs_and_unobserved_state_are_rejected(self) -> None:
        model = WorldModel(stream_id="stream-1")
        with self.assertRaises(WorldModelError):
            model.predict(predicted_at="2026-01-01T00:00:00Z")
        with self.assertRaises(WorldModelError):
            model.observe("", "event-1", "2026-01-01T00:00:00Z")
        with self.assertRaises(WorldModelError):
            prediction_error_to_salience(-1.0)

    def test_promoted_patterns_seed_only_symbolic_vocabulary(self) -> None:
        model = WorldModel(
            stream_id="pattern-stream",
            promoted_patterns=[
                {"pattern_id": "pattern-a", "status": "promoted", "confidence": 0.9},
                {"pattern_id": "pattern-b", "status": "promoted", "confidence": 0.8},
            ],
        )
        prediction = model.predict(predicted_at="2026-01-01T00:00:00Z")
        self.assertEqual(set(prediction.predicted_distribution), {"pattern-a", "pattern-b"})
        self.assertEqual(model.metrics()["promoted_pattern_count"], 2)
        with self.assertRaises(WorldModelError):
            WorldModel(
                stream_id="invalid-pattern-stream",
                promoted_patterns=[{"pattern_id": "candidate", "status": "candidate"}],
            )


if __name__ == "__main__":
    unittest.main()
