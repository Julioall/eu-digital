import ast
import sys
import unittest
from pathlib import Path

LAB_ROOT = Path(__file__).resolve().parents[1]
if str(LAB_ROOT) not in sys.path:
    sys.path.insert(0, str(LAB_ROOT))

from eu_digital_lab.digital_proprioception_agency import (
    ABLATION,
    AGENCY_POLICY_ID,
    BASELINE_POLICY_ID,
    FALSIFICATION,
    HYPOTHESIS,
    ActionIntention,
    AgencyActionOutcome,
    AgencyPolicy,
    AttributionLabel,
    DigitalAgencyEngine,
    DigitalAgencyError,
    DigitalBodyState,
    EfferenceCopy,
)

TIME_ZERO = "2026-07-29T12:00:00+00:00"
def intention(action_id: str = "action-1") -> ActionIntention:
    return ActionIntention(
        intention_id=f"intention-{action_id}",
        action_id=action_id,
        operation="create_note",
        target="local://notes/1",
        created_at=TIME_ZERO,
        predicted_effects=("note_created",),
        confidence=0.8,
        origin="supervised_action_controller",
    )


def own_outcome() -> AgencyActionOutcome:
    return AgencyActionOutcome(
        outcome_id="outcome-own",
        action_id="action-1",
        occurred_at="2026-07-29T12:00:01+00:00",
        observed_effects=("note_created",),
        success=True,
        observation_origin="unclassified",
        correlation_id="control-1",
    )


class DigitalProprioceptionAgencyTests(unittest.TestCase):
    def test_contracts_and_body_state_are_schema_backed(self) -> None:
        state = DigitalBodyState(
            state_id="body-1",
            occurred_at=TIME_ZERO,
            active_capabilities=("audio.speech",),
            queued_events=2,
            available_actions=("create_note",),
            initiated_actions=(),
            failures=(),
            limitations=("no_external_actuator",),
            latencies_ms={"event_bus": 1.5},
            avatar_state="quiet",
            action_origins={},
        )
        intention_value = intention()
        copy = EfferenceCopy(
            "copy-1", "action-1", TIME_ZERO, ("note_created",), 5000, "control-1"
        )
        state.to_mapping()
        intention_value.to_mapping()
        copy.to_mapping()
        own_outcome().to_mapping()
        self.assertEqual(copy.action_id, intention_value.action_id)

    def test_treatment_correlates_own_effect_and_records_prediction_error(self) -> None:
        engine = DigitalAgencyEngine()
        copy = engine.start(intention(), control_id="control-1", observation_window_ms=5000)
        assert copy is not None
        attribution = engine.observe(own_outcome())
        self.assertEqual(attribution.label, AttributionLabel.own)
        self.assertEqual(attribution.prediction_error, 0.0)
        self.assertEqual(attribution.policy_id, AGENCY_POLICY_ID)
        self.assertIn("efference_copy", attribution.evidence)

    def test_missing_correlation_remains_ambiguous(self) -> None:
        engine = DigitalAgencyEngine()
        engine.start(intention(), control_id="control-1", observation_window_ms=5000)
        outcome = AgencyActionOutcome(
            "outcome-uncertain",
            "action-1",
            "2026-07-29T12:00:01+00:00",
            ("note_created",),
            True,
            "unclassified",
            None,
        )
        attribution = engine.observe(outcome)
        self.assertEqual(attribution.label, AttributionLabel.ambiguous)
        self.assertNotEqual(attribution.label, AttributionLabel.external)

    def test_explicit_external_observation_can_be_external(self) -> None:
        engine = DigitalAgencyEngine()
        attribution = engine.observe(
            AgencyActionOutcome(
                "outcome-external",
                None,
                "2026-07-29T12:00:01+00:00",
                ("window_changed",),
                True,
                "explicit_external",
                None,
            )
        )
        self.assertEqual(attribution.action_id, "unmatched-outcome-external")
        self.assertEqual(attribution.label, AttributionLabel.external)

    def test_baseline_ablation_uses_same_interface_without_efference_copy(self) -> None:
        treatment = DigitalAgencyEngine()
        baseline = DigitalAgencyEngine(AgencyPolicy.passive_observer_v0)
        self.assertIsNotNone(
            treatment.start(intention(), control_id="control-1", observation_window_ms=5000)
        )
        self.assertIsNone(
            baseline.start(intention(), control_id="control-1", observation_window_ms=5000)
        )
        treatment.observe(own_outcome())
        baseline.observe(own_outcome())
        reference = {"action-1": AttributionLabel.own}
        self.assertGreater(
            float(treatment.metrics(reference)["macro_f1"]),
            float(baseline.metrics(reference)["macro_f1"]),
        )
        self.assertEqual(baseline.metrics(reference)["policy_id"], BASELINE_POLICY_ID)

    def test_scientific_metadata_and_deterministic_replay_are_explicit(self) -> None:
        first = DigitalAgencyEngine()
        second = DigitalAgencyEngine()
        for engine in (first, second):
            engine.start(intention(), control_id="control-1", observation_window_ms=5000)
            engine.observe(own_outcome())
        self.assertEqual(first.attributions[0].to_mapping(), second.attributions[0].to_mapping())
        metadata = DigitalAgencyEngine.scientific_metadata()
        self.assertEqual(metadata["hypothesis"], HYPOTHESIS)
        self.assertEqual(metadata["ablation"], ABLATION)
        self.assertEqual(metadata["falsification"], FALSIFICATION)

    def test_invalid_contracts_and_duplicate_actions_are_typed(self) -> None:
        with self.assertRaises(DigitalAgencyError):
            DigitalAgencyEngine().start(intention(), control_id="", observation_window_ms=0)
        engine = DigitalAgencyEngine()
        engine.start(intention(), control_id="control-1", observation_window_ms=5000)
        with self.assertRaises(DigitalAgencyError):
            engine.start(intention(), control_id="control-2", observation_window_ms=5000)

    def test_module_has_no_llm_or_external_service_import(self) -> None:
        source = (
            LAB_ROOT / "eu_digital_lab" / "digital_proprioception_agency.py"
        ).read_text(encoding="utf-8")
        tree = ast.parse(source)
        imports = [
            alias.name
            for node in ast.walk(tree)
            if isinstance(node, ast.Import)
            for alias in node.names
        ]
        imports.extend(
            node.module or "" for node in ast.walk(tree) if isinstance(node, ast.ImportFrom)
        )
        self.assertFalse(
            any(term in name.lower() for name in imports for term in ("llm", "requests", "http"))
        )


if __name__ == "__main__":
    unittest.main()
