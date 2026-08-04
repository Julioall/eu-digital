"""SPEC-043 — Reference implementation of the suggestion orchestrator.

This module provides the Python laboratory reference for the suggestive
orchestration engine. It does not execute actions, does not use semantic
fallback when the model is absent, and enforces all SPEC-043 policy limits.
"""

from __future__ import annotations

import hashlib
import json
import math
import uuid
from dataclasses import dataclass, field
from datetime import UTC, datetime, timedelta

SUGGESTION_SCHEMA_VERSION = "1.0"
SUGGESTION_POLICY_ID = "suggestive_orchestration_v1"
SUGGESTION_POLICY_VERSION = "1.0"
SUGGESTION_BASELINE_POLICY_ID = "fixed_delivery_v0"
SUGGESTION_CREATED_BY = "suggestion_orchestrator.local.v1"
SUGGESTION_NAMESPACE = uuid.UUID("b8f71e23-c4d9-4a1f-9e02-3d7a2f1c8b45")
SUGGESTION_HYPOTHESIS = (
    "suggestive orchestration with evidence, budget, cooldown and suppression "
    "reduces unjustified interruptions and improves correction rate versus "
    "fixed delivery"
)
SUGGESTION_ABLATION = (
    "disable metacognition, budget, cooldown, redundancy suppression, or "
    "switch to fixed_delivery_v0 baseline"
)
SUGGESTION_FALSIFICATION = (
    "correction rate worsens, interruptions exceed policy limits, evidence "
    "or explanation is missing, or any suggestion executes an action"
)


def _parse_timestamp(value: str) -> datetime:
    """Parse ISO-8601 timestamp to UTC datetime."""
    value = value.strip()
    if value.endswith("Z"):
        value = value[:-1] + "+00:00"
    return datetime.fromisoformat(value).astimezone(UTC)


def _format_timestamp(dt: datetime) -> str:
    """Format datetime to ISO-8601 UTC string matching C++ output."""
    utc = dt.astimezone(UTC)
    return utc.strftime("%Y-%m-%dT%H:%M:%S+00:00")


def _json_number(value: float) -> str:
    """Format float matching C++ to_chars output."""
    if not math.isfinite(value):
        raise ValueError("non-finite number cannot be serialized")
    formatted = f"{value}"
    if "." not in formatted and "e" not in formatted.lower():
        formatted += ".0"
    return formatted


def _policy_fingerprint(policy: SuggestionPolicy) -> str:
    """Compute deterministic policy fingerprint matching C++ digest."""
    obj = json.dumps(
        {
            "budget_enabled": policy.budget_enabled,
            "cooldown_enabled": policy.cooldown_enabled,
            "cooldown_seconds": float(policy.cooldown_seconds),
            "correction_cooldown_seconds": float(
                policy.correction_cooldown_seconds
            ),
            "max_per_day": policy.max_per_day,
            "max_per_window": policy.max_per_window,
            "min_confidence": float(policy.min_confidence),
            "min_information_gain": float(policy.min_information_gain),
            "policy_id": policy.policy_id,
            "policy_version": policy.policy_version,
            "redundancy_suppression": policy.redundancy_suppression,
            "window_seconds": float(policy.window_seconds),
        },
        separators=(",", ":"),
        sort_keys=True,
        ensure_ascii=True,
    )
    return hashlib.sha256(obj.encode()).hexdigest()[:16]


@dataclass
class SuggestionPolicy:
    """Configurable interruption policy per SPEC-043."""

    policy_id: str = SUGGESTION_POLICY_ID
    policy_version: str = SUGGESTION_POLICY_VERSION
    max_per_window: int = 3
    window_seconds: float = 900.0
    cooldown_seconds: float = 300.0
    correction_cooldown_seconds: float = 1800.0
    max_per_day: int = 8
    min_confidence: float = 0.15
    min_information_gain: float = 0.05
    redundancy_suppression: bool = True
    budget_enabled: bool = True
    cooldown_enabled: bool = True

    def validate(self) -> None:
        if not self.policy_id:
            raise ValueError("policy_id must be non-empty")
        if not self.policy_version:
            raise ValueError("policy_version must be non-empty")
        if self.max_per_window <= 0:
            raise ValueError("max_per_window must be positive")
        if self.max_per_day <= 0:
            raise ValueError("max_per_day must be positive")
        for name in ("window_seconds", "cooldown_seconds", "correction_cooldown_seconds"):
            val = getattr(self, name)
            if not math.isfinite(val) or val < 0:
                raise ValueError(f"{name} must be finite and non-negative")
        if not (0.0 <= self.min_confidence <= 1.0):
            raise ValueError("min_confidence must be between 0 and 1")
        if not (0.0 <= self.min_information_gain <= 1.0):
            raise ValueError("min_information_gain must be between 0 and 1")

    def fingerprint(self) -> str:
        self.validate()
        return _policy_fingerprint(self)


@dataclass
class SuggestionEvidence:
    """Input evidence for a suggestion candidate."""

    hypothesis_id: str
    confidence: float
    information_gain: float
    evidence_ids: list[str]
    reason: str

    def validate(self) -> None:
        if not self.hypothesis_id:
            raise ValueError("hypothesis_id must be non-empty")
        if not (0.0 <= self.confidence <= 1.0):
            raise ValueError("confidence must be between 0 and 1")
        if not math.isfinite(self.information_gain) or self.information_gain < 0:
            raise ValueError("information_gain must be finite and non-negative")
        if not self.reason:
            raise ValueError("reason must be non-empty")
        if not self.evidence_ids:
            raise ValueError("suggestion must carry at least one evidence reference")
        for ref in self.evidence_ids:
            if not ref:
                raise ValueError("evidence_id must be non-empty")


@dataclass
class SuggestionDecision:
    """A decision about whether to deliver a suggestion."""

    decision_id: str
    schema_version: str = SUGGESTION_SCHEMA_VERSION
    policy_id: str = SUGGESTION_POLICY_ID
    policy_version: str = SUGGESTION_POLICY_VERSION
    evidence_ids: list[str] = field(default_factory=list)
    hypothesis_id: str = ""
    confidence: float = 0.0
    information_gain: float = 0.0
    reason: str = ""
    suppressed: bool = False
    suppression_reason: str | None = None
    budget_before: int = 0
    budget_after: int = 0
    cooldown_remaining_seconds: float = 0.0
    override_active: bool = False
    action_proposed: bool = False
    created_at: str = ""

    def validate(self) -> None:
        if not self.decision_id:
            raise ValueError("decision_id must be non-empty")
        if self.schema_version != SUGGESTION_SCHEMA_VERSION:
            raise ValueError("unsupported suggestion schema version")
        if self.action_proposed:
            raise ValueError("SPEC-043 prohibits action proposals")
        if self.suppressed != (self.suppression_reason is not None):
            raise ValueError("suppression flag must match suppression_reason presence")
        if self.suppression_reason is not None and not self.suppression_reason:
            raise ValueError("suppression_reason cannot be empty")
        if not self.evidence_ids:
            raise ValueError("suggestion must carry evidence")
        if not self.reason:
            raise ValueError("reason must be non-empty")
        if not self.created_at:
            raise ValueError("created_at must be non-empty")

    def to_dict(self) -> dict:
        self.validate()
        return {
            "action_proposed": False,
            "budget_after": self.budget_after,
            "budget_before": self.budget_before,
            "confidence": self.confidence,
            "cooldown_remaining_seconds": self.cooldown_remaining_seconds,
            "created_at": self.created_at,
            "decision_id": self.decision_id,
            "evidence_ids": self.evidence_ids,
            "hypothesis_id": self.hypothesis_id,
            "information_gain": self.information_gain,
            "override_active": self.override_active,
            "policy_id": self.policy_id,
            "policy_version": self.policy_version,
            "reason": self.reason,
            "schema_version": self.schema_version,
            "suppressed": self.suppressed,
            "suppression_reason": self.suppression_reason,
        }


@dataclass
class SuggestionFeedbackRecord:
    """User feedback on a delivered suggestion."""

    feedback_id: str
    decision_id: str
    action: str  # "correct", "defer", "silence"
    correction: str | None = None
    occurred_at: str = ""

    def validate(self) -> None:
        if not self.feedback_id:
            raise ValueError("feedback_id must be non-empty")
        if not self.decision_id:
            raise ValueError("decision_id must be non-empty")
        if not self.occurred_at:
            raise ValueError("occurred_at must be non-empty")
        if self.action == "correct":
            if not self.correction:
                raise ValueError("correct feedback requires a correction")
        elif self.correction is not None:
            raise ValueError("only correct feedback accepts a correction")
        if self.action not in ("correct", "defer", "silence"):
            raise ValueError("unsupported suggestion feedback action")


class SuggestionOrchestrator:
    """SPEC-043 suggestion orchestration engine.

    Connects observations, episodes, memory, patterns, predictions,
    self-model and metacognition to explainable, correctable suggestions.
    Never executes actions.
    """

    def __init__(
        self,
        policy: SuggestionPolicy | None = None,
        model_available: bool = False,
    ) -> None:
        self._policy = policy or SuggestionPolicy()
        self._policy.validate()
        self._model_available = model_available
        self._decisions: list[SuggestionDecision] = []
        self._feedback_history: list[SuggestionFeedbackRecord] = []
        self._delivered_at: list[datetime] = []
        self._delivered_today: list[datetime] = []
        self._delivered_fingerprints: set[str] = set()
        self._cooldown_until: dict[str, datetime] = {}
        self._correction_count: dict[str, int] = {}
        self._total_feedback = 0
        self._accepted = 0
        self._silenced = 0

    @property
    def policy(self) -> SuggestionPolicy:
        return self._policy

    @property
    def model_available(self) -> bool:
        return self._model_available

    @model_available.setter
    def model_available(self, value: bool) -> None:
        self._model_available = value

    @property
    def decisions(self) -> list[SuggestionDecision]:
        return list(self._decisions)

    @property
    def feedback_history(self) -> list[SuggestionFeedbackRecord]:
        return list(self._feedback_history)

    def evaluate(self, evidence: SuggestionEvidence, now: str) -> SuggestionDecision:
        """Evaluate a suggestion candidate. Never executes actions."""
        evidence.validate()
        moment = _parse_timestamp(now)
        moment_str = _format_timestamp(moment)

        budget_before = self._remaining_budget(moment)
        suppression = self._compute_suppression(evidence, moment)

        budget_after = budget_before if suppression else max(0, budget_before - 1)
        cooldown_remaining = self._compute_cooldown_remaining(
            evidence.hypothesis_id, moment
        )

        decision_id = str(
            uuid.uuid5(
                SUGGESTION_NAMESPACE,
                f"{evidence.hypothesis_id}:{moment_str}:"
                f"{len(self._decisions)}:{self._policy.fingerprint()}",
            )
        )

        decision = SuggestionDecision(
            decision_id=decision_id,
            schema_version=SUGGESTION_SCHEMA_VERSION,
            policy_id=self._policy.policy_id,
            policy_version=self._policy.policy_version,
            evidence_ids=list(evidence.evidence_ids),
            hypothesis_id=evidence.hypothesis_id,
            confidence=evidence.confidence,
            information_gain=evidence.information_gain,
            reason=evidence.reason,
            suppressed=suppression is not None,
            suppression_reason=suppression,
            budget_before=budget_before,
            budget_after=budget_after,
            cooldown_remaining_seconds=cooldown_remaining,
            override_active=False,
            action_proposed=False,
            created_at=moment_str,
        )
        decision.validate()
        self._decisions.append(decision)

        if suppression is None:
            self._delivered_at.append(moment)
            self._delivered_today.append(moment)
            self._delivered_fingerprints.add(evidence.hypothesis_id)
            if self._policy.cooldown_enabled:
                self._cooldown_until[evidence.hypothesis_id] = moment + timedelta(
                    seconds=self._policy.cooldown_seconds
                )

        return decision

    def record_feedback(
        self,
        decision_id: str,
        action: str,
        correction: str | None,
        now: str,
    ) -> SuggestionFeedbackRecord:
        """Record user feedback on a delivered suggestion."""
        decision = self._find_decision(decision_id)
        if decision is None:
            raise ValueError("decision not found")
        if decision.suppressed:
            raise ValueError("cannot provide feedback on suppressed suggestions")

        moment = _parse_timestamp(now)
        moment_str = _format_timestamp(moment)

        feedback_id = str(
            uuid.uuid5(
                SUGGESTION_NAMESPACE,
                f"{decision_id}:{moment_str}:{action}",
            )
        )

        record = SuggestionFeedbackRecord(
            feedback_id=feedback_id,
            decision_id=decision_id,
            action=action,
            correction=correction,
            occurred_at=moment_str,
        )
        record.validate()

        if action == "correct":
            hyp = decision.hypothesis_id
            self._correction_count[hyp] = self._correction_count.get(hyp, 0) + 1
            if self._policy.cooldown_enabled:
                self._cooldown_until[hyp] = moment + timedelta(
                    seconds=self._policy.correction_cooldown_seconds
                )

        self._feedback_history.append(record)
        self._total_feedback += 1
        if action == "correct":
            self._accepted += 1
        if action == "silence":
            self._silenced += 1

        return record

    def evaluate_without_model(
        self, evidence: SuggestionEvidence, now: str
    ) -> SuggestionDecision:
        """Explicit degradation when model is absent."""
        if self._model_available:
            raise ValueError("model is available; use evaluate() instead")

        evidence.validate()
        moment = _parse_timestamp(now)
        moment_str = _format_timestamp(moment)

        decision_id = str(
            uuid.uuid5(
                SUGGESTION_NAMESPACE,
                f"no-model:{evidence.hypothesis_id}:{moment_str}:"
                f"{len(self._decisions)}",
            )
        )

        budget = self._remaining_budget(moment)
        decision = SuggestionDecision(
            decision_id=decision_id,
            schema_version=SUGGESTION_SCHEMA_VERSION,
            policy_id=self._policy.policy_id,
            policy_version=self._policy.policy_version,
            evidence_ids=list(evidence.evidence_ids),
            hypothesis_id=evidence.hypothesis_id,
            confidence=evidence.confidence,
            information_gain=evidence.information_gain,
            reason=(
                evidence.reason
                + " [model absent: explicit degradation, no semantic fallback]"
            ),
            suppressed=True,
            suppression_reason="model_absent",
            budget_before=budget,
            budget_after=budget,
            cooldown_remaining_seconds=0.0,
            override_active=False,
            action_proposed=False,
            created_at=moment_str,
        )
        decision.validate()
        self._decisions.append(decision)
        return decision

    def metrics(self) -> dict:
        """Return metrics snapshot for verification."""
        delivered = sum(1 for d in self._decisions if not d.suppressed)
        suppressed = len(self._decisions) - delivered
        return {
            "ablation": SUGGESTION_ABLATION,
            "accepted": self._accepted,
            "delivered": delivered,
            "falsification": SUGGESTION_FALSIFICATION,
            "hypothesis": SUGGESTION_HYPOTHESIS,
            "policy_fingerprint": self._policy.fingerprint(),
            "policy_id": self._policy.policy_id,
            "policy_version": self._policy.policy_version,
            "silenced": self._silenced,
            "suppressed": suppressed,
            "total_decisions": len(self._decisions),
            "total_feedback": self._total_feedback,
        }

    # --- Private ---

    def _remaining_budget(self, now: datetime) -> int:
        if not self._policy.budget_enabled:
            return self._policy.max_per_window

        window_start = now - timedelta(seconds=self._policy.window_seconds)
        window_count = sum(1 for t in self._delivered_at if t >= window_start)
        window_remaining = max(0, self._policy.max_per_window - window_count)

        day_start = now - timedelta(seconds=86400)
        day_count = sum(1 for t in self._delivered_today if t >= day_start)
        day_remaining = max(0, self._policy.max_per_day - day_count)

        return min(window_remaining, day_remaining)

    def _compute_cooldown_remaining(
        self, hypothesis_id: str, now: datetime
    ) -> float:
        if not self._policy.cooldown_enabled:
            return 0.0
        until = self._cooldown_until.get(hypothesis_id)
        if until is None:
            return 0.0
        return max(0.0, (until - now).total_seconds())

    def _compute_suppression(
        self, evidence: SuggestionEvidence, now: datetime
    ) -> str | None:
        if self._policy.budget_enabled and self._remaining_budget(now) <= 0:
            return "budget_exhausted"

        if self._policy.cooldown_enabled:
            until = self._cooldown_until.get(evidence.hypothesis_id)
            if until is not None and now < until:
                return "cooldown_active"

        if evidence.confidence < self._policy.min_confidence:
            return "confidence_below_minimum"

        if evidence.information_gain < self._policy.min_information_gain:
            return "information_gain_below_minimum"

        if (
            self._policy.redundancy_suppression
            and evidence.hypothesis_id in self._delivered_fingerprints
            and evidence.hypothesis_id not in self._correction_count
        ):
            return "redundant_hypothesis"

        return None

    def _find_decision(self, decision_id: str) -> SuggestionDecision | None:
        for d in self._decisions:
            if d.decision_id == decision_id:
                return d
        return None
