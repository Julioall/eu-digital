"""Tests for SPEC-043 suggestion orchestration reference."""

from __future__ import annotations

import json
import pathlib

import unittest

from reference.suggestion_orchestrator import (
    SUGGESTION_ABLATION,
    SUGGESTION_FALSIFICATION,
    SUGGESTION_HYPOTHESIS,
    SUGGESTION_SCHEMA_VERSION,
    SuggestionDecision,
    SuggestionEvidence,
    SuggestionFeedbackRecord,
    SuggestionOrchestrator,
    SuggestionPolicy,
)

CONTRACTS = pathlib.Path(__file__).resolve().parent.parent.parent / "contracts" / "schemas"


def _evidence(
    hypothesis_id: str = "h-001",
    confidence: float = 0.6,
    gain: float = 0.3,
    reason: str = "pattern observed",
) -> SuggestionEvidence:
    return SuggestionEvidence(
        hypothesis_id=hypothesis_id,
        confidence=confidence,
        information_gain=gain,
        evidence_ids=[f"ev-{hypothesis_id}-1"],
        reason=reason,
    )


class TestSuggestionDecisionSchema(unittest.TestCase):
    """Validate suggestion_decision.schema.json contract."""

    def test_schema_exists(self):
        schema_path = CONTRACTS / "suggestion_decision.schema.json"
        self.assertTrue(schema_path.exists(), "suggestion_decision.schema.json must exist")
        schema = json.loads(schema_path.read_text(encoding="utf-8"))
        self.assertEqual(schema["title"], "SuggestionDecision")
        self.assertIn("action_proposed", schema["properties"])
        # action_proposed must be false
        self.assertIs(schema["properties"]["action_proposed"]["const"], False)

    def test_required_fields(self):
        schema_path = CONTRACTS / "suggestion_decision.schema.json"
        schema = json.loads(schema_path.read_text(encoding="utf-8"))
        required = set(schema["required"])
        expected = {
            "decision_id", "schema_version", "policy_id", "policy_version",
            "evidence_ids", "hypothesis_id", "confidence", "information_gain",
            "reason", "suppressed", "suppression_reason", "budget_before",
            "budget_after", "cooldown_remaining_seconds", "override_active",
            "action_proposed", "created_at",
        }
        self.assertEqual(required, expected)


class TestSuggestionPolicy(unittest.TestCase):
    """Test policy validation and defaults."""

    def test_default_policy(self):
        p = SuggestionPolicy()
        p.validate()
        self.assertEqual(p.max_per_window, 3)
        self.assertEqual(p.window_seconds, 900.0)
        self.assertEqual(p.cooldown_seconds, 300.0)
        self.assertEqual(p.correction_cooldown_seconds, 1800.0)
        self.assertEqual(p.max_per_day, 8)

    def test_invalid_max_per_window(self):
        p = SuggestionPolicy(max_per_window=0)
        with self.assertRaisesRegex(ValueError, "max_per_window"):
            p.validate()

    def test_invalid_min_confidence(self):
        p = SuggestionPolicy(min_confidence=1.5)
        with self.assertRaisesRegex(ValueError, "min_confidence"):
            p.validate()

    def test_fingerprint_deterministic(self):
        p = SuggestionPolicy()
        self.assertEqual(p.fingerprint(), p.fingerprint())
        self.assertEqual(len(p.fingerprint()), 16)


class TestBasicSuggestion(unittest.TestCase):
    """Test basic suggestion delivery."""

    def test_deliver_suggestion(self):
        orch = SuggestionOrchestrator()
        d = orch.evaluate(_evidence(), "2026-07-31T12:00:00+00:00")
        self.assertTrue(d.decision_id)
        self.assertEqual(d.schema_version, SUGGESTION_SCHEMA_VERSION)
        self.assertEqual(d.hypothesis_id, "h-001")
        self.assertTrue(abs(d.confidence - 0.6) < 1e-9)
        self.assertTrue(abs(d.information_gain - 0.3) < 1e-9)
        self.assertFalse(d.suppressed)
        self.assertIs(d.suppression_reason, None)
        self.assertFalse(d.action_proposed)
        self.assertEqual(d.budget_before, 3)
        self.assertEqual(d.budget_after, 2)

    def test_action_proposed_always_false(self):
        d = SuggestionDecision(
            decision_id="test-1",
            evidence_ids=["e-1"],
            hypothesis_id="h-1",
            confidence=0.5,
            information_gain=0.1,
            reason="test",
            created_at="2026-07-31T12:00:00+00:00",
            action_proposed=True,
        )
        with self.assertRaisesRegex(ValueError, "SPEC-043 prohibits"):
            d.validate()

    def test_evidence_required(self):
        orch = SuggestionOrchestrator()
        evidence = SuggestionEvidence("h-001", 0.6, 0.3, [], "reason")
        with self.assertRaisesRegex(ValueError, "evidence"):
            orch.evaluate(evidence, "2026-07-31T12:00:00+00:00")


class TestBudget(unittest.TestCase):
    """Test budget exhaustion."""

    def test_window_budget(self):
        policy = SuggestionPolicy(max_per_window=2, redundancy_suppression=False)
        orch = SuggestionOrchestrator(policy)
        d1 = orch.evaluate(_evidence("h-001"), "2026-07-31T12:00:00+00:00")
        self.assertFalse(d1.suppressed)
        d2 = orch.evaluate(_evidence("h-002"), "2026-07-31T12:01:00+00:00")
        self.assertFalse(d2.suppressed)
        d3 = orch.evaluate(_evidence("h-003"), "2026-07-31T12:02:00+00:00")
        self.assertTrue(d3.suppressed)
        self.assertEqual(d3.suppression_reason, "budget_exhausted")

    def test_daily_budget(self):
        policy = SuggestionPolicy(
            max_per_window=100, max_per_day=2,
            redundancy_suppression=False, cooldown_enabled=False,
        )
        orch = SuggestionOrchestrator(policy)
        d1 = orch.evaluate(_evidence("h-001"), "2026-07-31T08:00:00+00:00")
        self.assertFalse(d1.suppressed)
        d2 = orch.evaluate(_evidence("h-002"), "2026-07-31T09:00:00+00:00")
        self.assertFalse(d2.suppressed)
        d3 = orch.evaluate(_evidence("h-003"), "2026-07-31T10:00:00+00:00")
        self.assertTrue(d3.suppressed)
        self.assertEqual(d3.suppression_reason, "budget_exhausted")


class TestCooldown(unittest.TestCase):
    """Test cooldown and correction cooldown."""

    def test_cooldown_active(self):
        policy = SuggestionPolicy(cooldown_seconds=300.0)
        orch = SuggestionOrchestrator(policy)
        d1 = orch.evaluate(_evidence("h-001"), "2026-07-31T12:00:00+00:00")
        self.assertFalse(d1.suppressed)
        d2 = orch.evaluate(_evidence("h-001"), "2026-07-31T12:03:00+00:00")
        self.assertTrue(d2.suppressed)
        self.assertEqual(d2.suppression_reason, "cooldown_active")
        self.assertTrue(d2.cooldown_remaining_seconds > 0)

    def test_correction_extends_cooldown(self):
        policy = SuggestionPolicy(
            cooldown_seconds=60.0, correction_cooldown_seconds=600.0,
            redundancy_suppression=False,
        )
        orch = SuggestionOrchestrator(policy)
        d1 = orch.evaluate(_evidence("h-001"), "2026-07-31T12:00:00+00:00")
        orch.record_feedback(d1.decision_id, "correct", "fix", "2026-07-31T12:00:10+00:00")
        d2 = orch.evaluate(_evidence("h-001"), "2026-07-31T12:05:00+00:00")
        self.assertTrue(d2.suppressed)
        self.assertEqual(d2.suppression_reason, "cooldown_active")


class TestSuppression(unittest.TestCase):
    """Test various suppression reasons."""

    def test_confidence_below_minimum(self):
        orch = SuggestionOrchestrator()
        d = orch.evaluate(_evidence(confidence=0.05), "2026-07-31T12:00:00+00:00")
        self.assertTrue(d.suppressed)
        self.assertEqual(d.suppression_reason, "confidence_below_minimum")

    def test_gain_below_minimum(self):
        orch = SuggestionOrchestrator()
        d = orch.evaluate(_evidence(gain=0.01), "2026-07-31T12:00:00+00:00")
        self.assertTrue(d.suppressed)
        self.assertEqual(d.suppression_reason, "information_gain_below_minimum")

    def test_redundancy(self):
        policy = SuggestionPolicy(cooldown_seconds=60.0)
        orch = SuggestionOrchestrator(policy)
        d1 = orch.evaluate(_evidence("h-001"), "2026-07-31T12:00:00+00:00")
        self.assertFalse(d1.suppressed)
        # After cooldown but redundant
        d2 = orch.evaluate(_evidence("h-001"), "2026-07-31T12:06:00+00:00")
        self.assertTrue(d2.suppressed)
        self.assertEqual(d2.suppression_reason, "redundant_hypothesis")

    def test_correction_resets_redundancy(self):
        policy = SuggestionPolicy(cooldown_seconds=60.0, correction_cooldown_seconds=120.0)
        orch = SuggestionOrchestrator(policy)
        d1 = orch.evaluate(_evidence("h-001"), "2026-07-31T12:00:00+00:00")
        orch.record_feedback(d1.decision_id, "correct", "wrong context", "2026-07-31T12:00:30+00:00")
        d2 = orch.evaluate(_evidence("h-001"), "2026-07-31T12:03:00+00:00")
        self.assertFalse(d2.suppressed)


class TestModelAbsent(unittest.TestCase):
    """Test explicit degradation without model."""

    def test_model_absent_suppresses(self):
        orch = SuggestionOrchestrator(model_available=False)
        d = orch.evaluate_without_model(_evidence(), "2026-07-31T12:00:00+00:00")
        self.assertTrue(d.suppressed)
        self.assertEqual(d.suppression_reason, "model_absent")
        self.assertIn("model absent", d.reason)
        self.assertFalse(d.action_proposed)

    def test_model_available_rejects_without_model(self):
        orch = SuggestionOrchestrator(model_available=True)
        with self.assertRaisesRegex(ValueError, "model is available"):
            orch.evaluate_without_model(_evidence(), "2026-07-31T12:00:00+00:00")


class TestFeedback(unittest.TestCase):
    """Test feedback recording."""

    def test_feedback_on_suppressed_rejected(self):
        policy = SuggestionPolicy(max_per_window=1, redundancy_suppression=False)
        orch = SuggestionOrchestrator(policy)
        orch.evaluate(_evidence("h-000"), "2026-07-31T12:00:00+00:00")
        d = orch.evaluate(_evidence("h-001"), "2026-07-31T12:00:30+00:00")
        self.assertTrue(d.suppressed)
        with self.assertRaisesRegex(ValueError, "suppressed"):
            orch.record_feedback(d.decision_id, "defer", None, "2026-07-31T12:01:00+00:00")

    def test_correct_requires_correction(self):
        orch = SuggestionOrchestrator()
        d = orch.evaluate(_evidence(), "2026-07-31T12:00:00+00:00")
        with self.assertRaisesRegex(ValueError, "correction"):
            orch.record_feedback(d.decision_id, "correct", None, "2026-07-31T12:01:00+00:00")

    def test_defer_rejects_correction(self):
        orch = SuggestionOrchestrator()
        d = orch.evaluate(_evidence(), "2026-07-31T12:00:00+00:00")
        with self.assertRaisesRegex(ValueError, "only correct"):
            orch.record_feedback(d.decision_id, "defer", "wrong", "2026-07-31T12:01:00+00:00")


class TestMetrics(unittest.TestCase):
    """Test metrics snapshot."""

    def test_metrics_content(self):
        orch = SuggestionOrchestrator()
        d = orch.evaluate(_evidence(), "2026-07-31T12:00:00+00:00")
        orch.record_feedback(d.decision_id, "defer", None, "2026-07-31T12:01:00+00:00")
        m = orch.metrics()
        self.assertEqual(m["delivered"], 1)
        self.assertEqual(m["total_decisions"], 1)
        self.assertEqual(m["total_feedback"], 1)
        self.assertEqual(m["hypothesis"], SUGGESTION_HYPOTHESIS)
        self.assertEqual(m["falsification"], SUGGESTION_FALSIFICATION)
        self.assertEqual(m["ablation"], SUGGESTION_ABLATION)


class TestAblation(unittest.TestCase):
    """Test ablation: no budget, no cooldown, no redundancy."""

    def test_unlimited_without_budget(self):
        policy = SuggestionPolicy(
            budget_enabled=False, cooldown_enabled=False,
            redundancy_suppression=False,
        )
        orch = SuggestionOrchestrator(policy)
        for i in range(10):
            d = orch.evaluate(
                _evidence(f"h-{i:03d}"),
                f"2026-07-31T12:0{i}:00+00:00",
            )
            self.assertFalse(d.suppressed)


class TestSerialization(unittest.TestCase):
    """Test JSON serialization."""

    def test_decision_dict(self):
        orch = SuggestionOrchestrator()
        d = orch.evaluate(_evidence(), "2026-07-31T12:00:00+00:00")
        data = d.to_dict()
        self.assertIs(data["action_proposed"], False)
        self.assertEqual(data["hypothesis_id"], "h-001")
        self.assertEqual(data["schema_version"], "1.0")
        self.assertIs(data["suppressed"], False)


class TestPlugin(unittest.TestCase):
    """Test the plugin descriptor pattern."""

    def test_orchestrator_as_capability(self):
        # The orchestrator follows the same removable pattern
        orch = SuggestionOrchestrator()
        # Verify it doesn't import concrete plugins
        self.assertTrue(hasattr(orch, "evaluate"))
        self.assertTrue(hasattr(orch, "evaluate_without_model"))
        self.assertTrue(hasattr(orch, "record_feedback"))
        self.assertTrue(hasattr(orch, "metrics"))
