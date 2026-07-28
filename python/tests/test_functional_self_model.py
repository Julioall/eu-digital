import ast
import sys
import unittest
from pathlib import Path

LAB_ROOT = Path(__file__).resolve().parents[1]
if str(LAB_ROOT) not in sys.path:
    sys.path.insert(0, str(LAB_ROOT))

from eu_digital_lab.functional_self_model import (
    ABLATION,
    BASELINE_POLICY_ID,
    FALSIFICATION,
    HYPOTHESIS,
    SELF_MODEL_POLICY_ID,
    AssertionClassification,
    CapabilityStatus,
    DecisionPolicy,
    FunctionalSelfModelError,
    SelfModelAssertion,
    SelfModelEvent,
    SelfModelEventKind,
    VersionedFunctionalSelfModel,
)

TIME_ZERO = "2026-07-28T12:00:00+00:00"


def capability_event(
    event_id: str,
    capability_id: str,
    status: CapabilityStatus,
    *,
    occurred_at: str = TIME_ZERO,
) -> SelfModelEvent:
    return SelfModelEvent(
        event_id=event_id,
        occurred_at=occurred_at,
        kind=SelfModelEventKind.capability_changed,
        reason=f"{capability_id} is {status.value}",
        source_event_ids=(f"source-{event_id}",),
        capability_id=capability_id,
        capability_status=status,
        capability_explanation=f"Capability {capability_id} is declared {status.value}.",
        assertion=None,
    )


def assertion_event(
    event_id: str,
    classification: AssertionClassification,
    *,
    occurred_at: str = TIME_ZERO,
) -> SelfModelEvent:
    assertion = SelfModelAssertion(
        assertion_id=f"assertion-{event_id}",
        subject="local-agent",
        predicate="mode",
        value=f"value-{classification.value}",
        classification=classification,
        explanation=f"Recorded {classification.value} for test.",
        source_event_ids=(f"source-{event_id}",),
    )
    return SelfModelEvent(
        event_id=event_id,
        occurred_at=occurred_at,
        kind=SelfModelEventKind.assertion_recorded,
        reason=f"record {classification.value}",
        source_event_ids=(f"source-{event_id}",),
        capability_id=None,
        capability_status=None,
        capability_explanation=None,
        assertion=assertion,
    )


class FunctionalSelfModelTests(unittest.TestCase):
    def model(
        self, policy: DecisionPolicy = DecisionPolicy.self_model_gate_v1
    ) -> VersionedFunctionalSelfModel:
        return VersionedFunctionalSelfModel(
            model_id="local-agent",
            initial_at=TIME_ZERO,
            decision_policy=policy,
        )

    def test_capability_change_creates_new_version_and_preserves_prior(self) -> None:
        model = self.model()
        initial = model.current
        available = model.apply(
            capability_event("capability-available", "screen.capture", CapabilityStatus.available)
        )
        unavailable = model.apply(
            capability_event(
                "capability-unavailable",
                "screen.capture",
                CapabilityStatus.unavailable,
                occurred_at="2026-07-28T12:00:01+00:00",
            )
        )

        self.assertEqual(initial.version, 0)
        self.assertEqual(available.version, 1)
        self.assertEqual(unavailable.version, 2)
        self.assertEqual(model.version(1).capabilities[0].status, CapabilityStatus.available)
        self.assertEqual(model.version(2).capabilities[0].status, CapabilityStatus.unavailable)
        self.assertEqual(model.version(0).capabilities, ())

    def test_assertions_keep_fact_hypothesis_and_configuration_separate(self) -> None:
        model = self.model()
        model.apply(assertion_event("fact", AssertionClassification.fact))
        model.apply(
            assertion_event(
                "hypothesis",
                AssertionClassification.hypothesis,
                occurred_at="2026-07-28T12:00:01+00:00",
            )
        )
        snapshot = model.apply(
            assertion_event(
                "configuration",
                AssertionClassification.configuration,
                occurred_at="2026-07-28T12:00:02+00:00",
            )
        )

        self.assertEqual([item.classification for item in snapshot.facts], [AssertionClassification.fact])
        self.assertEqual(
            [item.classification for item in snapshot.hypotheses],
            [AssertionClassification.hypothesis],
        )
        self.assertEqual(
            [item.classification for item in snapshot.configuration],
            [AssertionClassification.configuration],
        )

    def test_decisions_use_current_capability_state_and_explain_limitations(self) -> None:
        model = self.model()
        unknown = model.decide("screen.capture")
        model.apply(
            capability_event("available", "screen.capture", CapabilityStatus.available)
        )
        available = model.decide("screen.capture")
        model.apply(
            capability_event(
                "degraded",
                "screen.capture",
                CapabilityStatus.degraded,
                occurred_at="2026-07-28T12:00:01+00:00",
            )
        )
        degraded = model.decide("screen.capture")

        self.assertFalse(unknown.allowed)
        self.assertEqual(unknown.reason_code, "capability_unverified")
        self.assertTrue(available.allowed)
        self.assertEqual(available.reason_code, "capability_available")
        self.assertFalse(degraded.allowed)
        self.assertEqual(degraded.reason_code, "capability_degraded")
        self.assertIn("declared degraded", degraded.explanation)

    def test_unavailable_and_removed_capabilities_are_explained_distinctly(self) -> None:
        model = self.model()
        model.apply(
            capability_event("unavailable", "input.read", CapabilityStatus.unavailable)
        )
        unavailable = model.decide("input.read")
        model.apply(
            capability_event(
                "removed",
                "input.read",
                CapabilityStatus.removed,
                occurred_at="2026-07-28T12:00:01+00:00",
            )
        )
        removed = model.decide("input.read")

        self.assertEqual(unavailable.reason_code, "capability_unavailable")
        self.assertEqual(removed.reason_code, "capability_removed")
        self.assertNotEqual(unavailable.explanation, removed.explanation)

    def test_baseline_ablation_does_not_consult_snapshot(self) -> None:
        treatment = self.model()
        baseline = self.model(DecisionPolicy.unconstrained_decision_v0)
        event = capability_event("loss", "screen.capture", CapabilityStatus.unavailable)
        treatment.apply(event)
        baseline.apply(event)

        treatment_decision = treatment.decide("screen.capture")
        baseline_decision = baseline.decide("screen.capture")

        self.assertFalse(treatment_decision.allowed)
        self.assertTrue(baseline_decision.allowed)
        self.assertEqual(treatment_decision.policy_id, SELF_MODEL_POLICY_ID)
        self.assertEqual(baseline_decision.policy_id, BASELINE_POLICY_ID)

    def test_contracts_hash_chain_and_decisions_are_auditable(self) -> None:
        model = self.model()
        snapshot = model.apply(
            capability_event("available", "screen.capture", CapabilityStatus.available)
        )
        decision = model.decide("screen.capture")

        self.assertEqual(snapshot.to_mapping()["version"], 1)
        self.assertTrue(snapshot.history_hash)
        self.assertEqual(decision.to_mapping()["snapshot_id"], snapshot.snapshot_id)
        self.assertEqual(model.metrics()["history_version_count"], 2)

    def test_same_event_replay_is_deterministic_and_has_no_llm_dependency(self) -> None:
        events = (
            capability_event("available", "screen.capture", CapabilityStatus.available),
            assertion_event(
                "configuration",
                AssertionClassification.configuration,
                occurred_at="2026-07-28T12:00:01+00:00",
            ),
        )
        first = self.model()
        second = self.model()
        for event in events:
            first.apply(event)
            second.apply(event)

        source = (LAB_ROOT / "eu_digital_lab" / "functional_self_model.py").read_text(
            encoding="utf-8"
        )
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

        self.assertEqual(first.snapshot(), second.snapshot())
        self.assertFalse(any("llm" in name.lower() for name in imports))

    def test_invalid_event_and_history_access_raise_typed_errors(self) -> None:
        model = self.model()
        with self.assertRaises(FunctionalSelfModelError):
            SelfModelEvent(
                event_id="bad",
                occurred_at=TIME_ZERO,
                kind=SelfModelEventKind.capability_changed,
                reason="missing capability",
                source_event_ids=("source-bad",),
                capability_id=None,
                capability_status=None,
                capability_explanation=None,
                assertion=None,
            )
        with self.assertRaises(FunctionalSelfModelError):
            model.version(5)
        event = capability_event("once", "screen.capture", CapabilityStatus.available)
        model.apply(event)
        with self.assertRaises(FunctionalSelfModelError):
            model.apply(event)

    def test_scientific_metadata_is_registered(self) -> None:
        metrics = self.model().metrics()

        self.assertEqual(metrics["hypothesis"], HYPOTHESIS)
        self.assertEqual(metrics["ablation"], ABLATION)
        self.assertEqual(metrics["falsification"], FALSIFICATION)
        self.assertEqual(metrics["baseline_policy_id"], BASELINE_POLICY_ID)


if __name__ == "__main__":
    unittest.main()
