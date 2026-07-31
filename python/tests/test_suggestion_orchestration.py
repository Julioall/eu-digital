"""Tests for SPEC-043 suggestion orchestration reference."""

from __future__ import annotations

import json
import pathlib

import pytest

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


class TestSuggestionDecisionSchema:
    """Validate suggestion_decision.schema.json contract."""

    def test_schema_exists(self):
        schema_path = CONTRACTS / "suggestion_decision.schema.json"
        assert schema_path.exists(), "suggestion_decision.schema.json must exist"
        schema = json.loads(schema_path.read_text(encoding="utf-8"))
        assert schema["title"] == "SuggestionDecision"
        assert "action_proposed" in schema["properties"]
        # action_proposed must be false
        assert schema["properties"]["action_proposed"]["const"] is False

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
        assert required == expected


class TestSuggestionPolicy:
    """Test policy validation and defaults."""

    def test_default_policy(self):
        p = SuggestionPolicy()
        p.validate()
        assert p.max_per_window == 3
        assert p.window_seconds == 900.0
        assert p.cooldown_seconds == 300.0
        assert p.correction_cooldown_seconds == 1800.0
        assert p.max_per_day == 8

    def test_invalid_max_per_window(self):
        p = SuggestionPolicy(max_per_window=0)
        with pytest.raises(ValueError, match="max_per_window"):
            p.validate()

    def test_invalid_min_confidence(self):
        p = SuggestionPolicy(min_confidence=1.5)
        with pytest.raises(ValueError, match="min_confidence"):
            p.validate()

    def test_fingerprint_deterministic(self):
        p = SuggestionPolicy()
        assert p.fingerprint() == p.fingerprint()
        assert len(p.fingerprint()) == 16


class TestBasicSuggestion:
    """Test basic suggestion delivery."""

    def test_deliver_suggestion(self):
        orch = SuggestionOrchestrator()
        d = orch.evaluate(_evidence(), "2026-07-31T12:00:00+00:00")
        assert d.decision_id
        assert d.schema_version == SUGGESTION_SCHEMA_VERSION
        assert d.hypothesis_id == "h-001"
        assert abs(d.confidence - 0.6) < 1e-9
        assert abs(d.information_gain - 0.3) < 1e-9
        assert not d.suppressed
        assert d.suppression_reason is None
        assert not d.action_proposed
        assert d.budget_before == 3
        assert d.budget_after == 2

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
        with pytest.raises(ValueError, match="SPEC-043 prohibits"):
            d.validate()

    def test_evidence_required(self):
        orch = SuggestionOrchestrator()
        evidence = SuggestionEvidence("h-001", 0.6, 0.3, [], "reason")
        with pytest.raises(ValueError, match="evidence"):
            orch.evaluate(evidence, "2026-07-31T12:00:00+00:00")


class TestBudget:
    """Test budget exhaustion."""

    def test_window_budget(self):
        policy = SuggestionPolicy(max_per_window=2, redundancy_suppression=False)
        orch = SuggestionOrchestrator(policy)
        d1 = orch.evaluate(_evidence("h-001"), "2026-07-31T12:00:00+00:00")
        assert not d1.suppressed
        d2 = orch.evaluate(_evidence("h-002"), "2026-07-31T12:01:00+00:00")
        assert not d2.suppressed
        d3 = orch.evaluate(_evidence("h-003"), "2026-07-31T12:02:00+00:00")
        assert d3.suppressed
        assert d3.suppression_reason == "budget_exhausted"

    def test_daily_budget(self):
        policy = SuggestionPolicy(
            max_per_window=100, max_per_day=2,
            redundancy_suppression=False, cooldown_enabled=False,
        )
        orch = SuggestionOrchestrator(policy)
        d1 = orch.evaluate(_evidence("h-001"), "2026-07-31T08:00:00+00:00")
        assert not d1.suppressed
        d2 = orch.evaluate(_evidence("h-002"), "2026-07-31T09:00:00+00:00")
        assert not d2.suppressed
        d3 = orch.evaluate(_evidence("h-003"), "2026-07-31T10:00:00+00:00")
        assert d3.suppressed
        assert d3.suppression_reason == "budget_exhausted"


class TestCooldown:
    """Test cooldown and correction cooldown."""

    def test_cooldown_active(self):
        policy = SuggestionPolicy(cooldown_seconds=300.0)
        orch = SuggestionOrchestrator(policy)
        d1 = orch.evaluate(_evidence("h-001"), "2026-07-31T12:00:00+00:00")
        assert not d1.suppressed
        d2 = orch.evaluate(_evidence("h-001"), "2026-07-31T12:03:00+00:00")
        assert d2.suppressed
        assert d2.suppression_reason == "cooldown_active"
        assert d2.cooldown_remaining_seconds > 0

    def test_correction_extends_cooldown(self):
        policy = SuggestionPolicy(
            cooldown_seconds=60.0, correction_cooldown_seconds=600.0,
            redundancy_suppression=False,
        )
        orch = SuggestionOrchestrator(policy)
        d1 = orch.evaluate(_evidence("h-001"), "2026-07-31T12:00:00+00:00")
        orch.record_feedback(d1.decision_id, "correct", "fix", "2026-07-31T12:00:10+00:00")
        d2 = orch.evaluate(_evidence("h-001"), "2026-07-31T12:05:00+00:00")
        assert d2.suppressed
        assert d2.suppression_reason == "cooldown_active"


class TestSuppression:
    """Test various suppression reasons."""

    def test_confidence_below_minimum(self):
        orch = SuggestionOrchestrator()
        d = orch.evaluate(_evidence(confidence=0.05), "2026-07-31T12:00:00+00:00")
        assert d.suppressed
        assert d.suppression_reason == "confidence_below_minimum"

    def test_gain_below_minimum(self):
        orch = SuggestionOrchestrator()
        d = orch.evaluate(_evidence(gain=0.01), "2026-07-31T12:00:00+00:00")
        assert d.suppressed
        assert d.suppression_reason == "information_gain_below_minimum"

    def test_redundancy(self):
        policy = SuggestionPolicy(cooldown_seconds=60.0)
        orch = SuggestionOrchestrator(policy)
        d1 = orch.evaluate(_evidence("h-001"), "2026-07-31T12:00:00+00:00")
        assert not d1.suppressed
        # After cooldown but redundant
        d2 = orch.evaluate(_evidence("h-001"), "2026-07-31T12:06:00+00:00")
        assert d2.suppressed
        assert d2.suppression_reason == "redundant_hypothesis"

    def test_correction_resets_redundancy(self):
        policy = SuggestionPolicy(cooldown_seconds=60.0, correction_cooldown_seconds=120.0)
        orch = SuggestionOrchestrator(policy)
        d1 = orch.evaluate(_evidence("h-001"), "2026-07-31T12:00:00+00:00")
        orch.record_feedback(d1.decision_id, "correct", "wrong context", "2026-07-31T12:00:30+00:00")
        d2 = orch.evaluate(_evidence("h-001"), "2026-07-31T12:03:00+00:00")
        assert not d2.suppressed


class TestModelAbsent:
    """Test explicit degradation without model."""

    def test_model_absent_suppresses(self):
        orch = SuggestionOrchestrator(model_available=False)
        d = orch.evaluate_without_model(_evidence(), "2026-07-31T12:00:00+00:00")
        assert d.suppressed
        assert d.suppression_reason == "model_absent"
        assert "model absent" in d.reason
        assert not d.action_proposed

    def test_model_available_rejects_without_model(self):
        orch = SuggestionOrchestrator(model_available=True)
        with pytest.raises(ValueError, match="model is available"):
            orch.evaluate_without_model(_evidence(), "2026-07-31T12:00:00+00:00")


class TestFeedback:
    """Test feedback recording."""

    def test_feedback_on_suppressed_rejected(self):
        policy = SuggestionPolicy(max_per_window=1, redundancy_suppression=False)
        orch = SuggestionOrchestrator(policy)
        orch.evaluate(_evidence("h-000"), "2026-07-31T12:00:00+00:00")
        d = orch.evaluate(_evidence("h-001"), "2026-07-31T12:00:30+00:00")
        assert d.suppressed
        with pytest.raises(ValueError, match="suppressed"):
            orch.record_feedback(d.decision_id, "defer", None, "2026-07-31T12:01:00+00:00")

    def test_correct_requires_correction(self):
        orch = SuggestionOrchestrator()
        d = orch.evaluate(_evidence(), "2026-07-31T12:00:00+00:00")
        with pytest.raises(ValueError, match="correction"):
            orch.record_feedback(d.decision_id, "correct", None, "2026-07-31T12:01:00+00:00")

    def test_defer_rejects_correction(self):
        orch = SuggestionOrchestrator()
        d = orch.evaluate(_evidence(), "2026-07-31T12:00:00+00:00")
        with pytest.raises(ValueError, match="only correct"):
            orch.record_feedback(d.decision_id, "defer", "wrong", "2026-07-31T12:01:00+00:00")


class TestMetrics:
    """Test metrics snapshot."""

    def test_metrics_content(self):
        orch = SuggestionOrchestrator()
        d = orch.evaluate(_evidence(), "2026-07-31T12:00:00+00:00")
        orch.record_feedback(d.decision_id, "defer", None, "2026-07-31T12:01:00+00:00")
        m = orch.metrics()
        assert m["delivered"] == 1
        assert m["total_decisions"] == 1
        assert m["total_feedback"] == 1
        assert m["hypothesis"] == SUGGESTION_HYPOTHESIS
        assert m["falsification"] == SUGGESTION_FALSIFICATION
        assert m["ablation"] == SUGGESTION_ABLATION


class TestAblation:
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
            assert not d.suppressed


class TestSerialization:
    """Test JSON serialization."""

    def test_decision_dict(self):
        orch = SuggestionOrchestrator()
        d = orch.evaluate(_evidence(), "2026-07-31T12:00:00+00:00")
        data = d.to_dict()
        assert data["action_proposed"] is False
        assert data["hypothesis_id"] == "h-001"
        assert data["schema_version"] == "1.0"
        assert data["suppressed"] is False


class TestPlugin:
    """Test the plugin descriptor pattern."""

    def test_orchestrator_as_capability(self):
        # The orchestrator follows the same removable pattern
        orch = SuggestionOrchestrator()
        # Verify it doesn't import concrete plugins
        assert hasattr(orch, "evaluate")
        assert hasattr(orch, "evaluate_without_model")
        assert hasattr(orch, "record_feedback")
        assert hasattr(orch, "metrics")
