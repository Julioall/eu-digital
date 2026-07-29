import sys
import unittest
from pathlib import Path

LAB_ROOT = Path(__file__).resolve().parents[1]
if str(LAB_ROOT) not in sys.path:
    sys.path.insert(0, str(LAB_ROOT))

from eu_digital_lab.capability_adaptation import (
    CapabilityAdaptationEngine,
    CapabilityAdaptationError,
)


class CapabilityAdaptationTests(unittest.TestCase):
    def engine(self) -> CapabilityAdaptationEngine:
        engine = CapabilityAdaptationEngine(
            agent_id="agent-1",
            attention_weights={"system": 0.5, "vision": 0.3, "audio": 0.2},
            calibration_samples=3,
        )
        engine.register_capability("system-sensor", "system", state="available")
        engine.register_capability("vision-sensor", "vision", state="available")
        engine.register_capability("audio-sensor", "audio", state="available")
        return engine

    def test_removing_vision_reduces_visual_confidence_and_redistributes_attention(self) -> None:
        engine = self.engine()
        engine.register_belief("visual-belief", ("vision",), 0.9)
        engine.register_prediction("visual-prediction", ("vision",))

        event = engine.apply_change(
            capability_id="vision-sensor",
            old_state="available",
            new_state="removed",
            observed_at="2026-01-01T00:00:00+00:00",
        )
        profile = engine.profile()

        self.assertEqual(event.invalidated_prediction_ids, ("visual-prediction",))
        self.assertLess(event.confidence_adjustments["visual-belief"], 0.9)
        self.assertNotIn("vision", profile["attention_weights"])
        self.assertIn("partial_observability", profile["limitation_codes"])
        self.assertTrue(event.source_observation_present)

    def test_audio_removal_does_not_invalidate_system_prediction(self) -> None:
        engine = self.engine()
        engine.register_prediction("system-episode", ("system",))

        event = engine.apply_change(
            capability_id="audio-sensor",
            old_state="available",
            new_state="temporarily_unavailable",
            observed_at="2026-01-01T00:00:00+00:00",
        )

        self.assertEqual(event.invalidated_prediction_ids, ())
        self.assertIn("system", engine.profile()["available_modalities"])

    def test_absent_actuator_blocks_plan(self) -> None:
        engine = self.engine()
        engine.register_capability("keyboard-actuator", "keyboard", state="available")
        engine.register_plan("close-window", ("keyboard-actuator",))
        event = engine.apply_change(
            capability_id="keyboard-actuator",
            old_state="available",
            new_state="removed",
            observed_at="2026-01-01T00:00:00+00:00",
        )

        self.assertEqual(event.blocked_plan_ids, ("close-window",))
        self.assertEqual(engine.metrics()["blocked_plan_count"], 1)

    def test_new_modality_calibrates_before_stable_influence(self) -> None:
        engine = self.engine()
        engine.register_capability("depth-sensor", "depth", state="calibrating")
        first = engine.calibrate("depth-sensor", 1, "2026-01-01T00:00:00+00:00")
        self.assertFalse(first.stable_influence)
        self.assertNotIn("depth", engine.profile()["attention_weights"])
        stable = engine.calibrate("depth-sensor", 2, "2026-01-01T00:00:01+00:00")

        self.assertTrue(stable.stable_influence)
        self.assertIn("depth", engine.profile()["attention_weights"])

    def test_returning_capability_preserves_agent_and_history(self) -> None:
        engine = self.engine()
        engine.apply_change(
            capability_id="vision-sensor",
            old_state="available",
            new_state="removed",
            observed_at="2026-01-01T00:00:00+00:00",
        )
        engine.apply_change(
            capability_id="vision-sensor",
            old_state="removed",
            new_state="available",
            observed_at="2026-01-01T00:00:01+00:00",
        )
        profile = engine.profile()

        self.assertEqual(profile["agent_id"], "agent-1")
        self.assertEqual(profile["identity_generation"], 1)
        self.assertIn("vision-sensor", profile["capability_history"])
        self.assertIn("vision", profile["available_modalities"])

    def test_adaptive_attention_beats_fixed_attention_ablation(self) -> None:
        engine = self.engine()
        engine.apply_change(
            capability_id="vision-sensor",
            old_state="available",
            new_state="removed",
            observed_at="2026-01-01T00:00:00+00:00",
        )
        metrics = engine.metrics()

        self.assertGreater(metrics["adaptive_observable_attention"], metrics["fixed_observable_attention"])
        self.assertGreater(metrics["attention_gain"], 0.0)

    def test_invalid_change_and_calibration_are_typed_errors(self) -> None:
        engine = self.engine()
        with self.assertRaises(CapabilityAdaptationError):
            engine.apply_change(
                capability_id="vision-sensor",
                old_state="removed",
                new_state="available",
                observed_at="2026-01-01T00:00:00+00:00",
            )
        with self.assertRaises(CapabilityAdaptationError):
            engine.calibrate("vision-sensor", 1, "2026-01-01T00:00:00+00:00")


if __name__ == "__main__":
    unittest.main()
