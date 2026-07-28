"""Calibrated metacognition and bounded curiosity for the local research lab.

The module evaluates explicit hypotheses and produces structured question
proposals. It neither invokes a language model nor sends questions externally;
confidence is an operational estimate calibrated only by recorded outcomes.
"""

from __future__ import annotations

import hashlib
import json
import math
import uuid
from collections.abc import Collection, Mapping, Sequence
from dataclasses import dataclass, replace
from datetime import UTC, datetime, timedelta
from enum import Enum
from typing import Any

from .schema_validation import validate_shared_schema

SCHEMA_VERSION = "1.0"
RAW_CONFIDENCE_ID = "evidence_ratio_v1"
CALIBRATOR_ID = "bucketed_beta_v1"
INFORMATION_GAIN_POLICY_ID = "information_gain_v1"
BASELINE_CONFIDENCE_ID = "raw_confidence_v0"
BASELINE_QUESTION_POLICY_ID = "fixed_gain_v0"
CREATED_BY = "metacognition_curiosity.local.v1"
HYPOTHESIS = (
    "outcome-calibrated confidence and information-gain selection reduce "
    "unjustified and redundant question proposals versus fixed controls"
)
ABLATION = (
    "disable outcome calibration, select fixed_gain_v0, and disable budget, "
    "cooldown, or redundancy suppression"
)
FALSIFICATION = (
    "confidence remains decoupled from verified outcomes, or information-gain "
    "selection does not improve gain, redundancy, and interruption cost over controls"
)
HYPOTHESIS_KINDS = frozenset(
    {"observed_pattern", "causal", "predictive", "contextual", "capability"}
)
_NAMESPACE = uuid.UUID("26da5c19-e611-4ebb-a26e-5b341d2df708")


class MetacognitionCuriosityError(ValueError):
    """Raised for invalid contracts, outcomes, or question state transitions."""


class HypothesisStatus(str, Enum):
    proposed = "proposed"
    confirmed = "confirmed"
    rejected = "rejected"
    superseded = "superseded"


class QuestionPolicy(str, Enum):
    information_gain_v1 = INFORMATION_GAIN_POLICY_ID
    fixed_gain_v0 = BASELINE_QUESTION_POLICY_ID


class QuestionStatus(str, Enum):
    proposed = "proposed"
    suppressed = "suppressed"
    asked = "asked"
    answered = "answered"


class ResponseOutcome(str, Enum):
    confirmed = "confirmed"
    rejected = "rejected"
    inconclusive = "inconclusive"


@dataclass(frozen=True)
class HypothesisRecord:
    """Versioned hypothesis with provenance and explicit counter-evidence."""

    hypothesis_id: str
    kind: str
    statement: str
    status: HypothesisStatus
    confidence: float
    supporting_refs: tuple[str, ...]
    opposing_refs: tuple[str, ...]
    alternatives: tuple[str, ...]
    created_at: str
    updated_at: str
    verification_question: str | None
    expected_information_gain: float | None
    provenance_module: str
    model_version: str | None
    schema_version: str = SCHEMA_VERSION

    def __post_init__(self) -> None:
        _required_string(self.hypothesis_id, "hypothesis_id")
        if self.kind not in HYPOTHESIS_KINDS:
            raise MetacognitionCuriosityError("unsupported hypothesis kind")
        _required_string(self.statement, "statement")
        try:
            object.__setattr__(self, "status", HypothesisStatus(self.status))
        except ValueError as error:
            raise MetacognitionCuriosityError("unsupported hypothesis status") from error
        _probability(self.confidence, "confidence")
        _parse_time(self.created_at, "created_at")
        _parse_time(self.updated_at, "updated_at")
        if _parse_time(self.updated_at, "updated_at") < _parse_time(self.created_at, "created_at"):
            raise MetacognitionCuriosityError("updated_at cannot precede created_at")
        object.__setattr__(self, "supporting_refs", _references(self.supporting_refs, "supporting_refs"))
        object.__setattr__(self, "opposing_refs", _references(self.opposing_refs, "opposing_refs"))
        object.__setattr__(self, "alternatives", _references(self.alternatives, "alternatives"))
        if self.verification_question is not None:
            _required_string(self.verification_question, "verification_question")
        if self.expected_information_gain is not None:
            _probability(self.expected_information_gain, "expected_information_gain")
        _required_string(self.provenance_module, "provenance_module")
        if self.model_version is not None:
            _required_string(self.model_version, "model_version")
        if self.schema_version != SCHEMA_VERSION:
            raise MetacognitionCuriosityError("unsupported hypothesis schema version")
        self.to_mapping()

    def to_mapping(self) -> dict[str, Any]:
        value = {
            "hypothesis_id": self.hypothesis_id,
            "schema_version": self.schema_version,
            "kind": self.kind,
            "statement": self.statement,
            "status": self.status.value,
            "confidence": self.confidence,
            "evidence": {
                "supporting_refs": list(self.supporting_refs),
                "opposing_refs": list(self.opposing_refs),
            },
            "alternatives": list(self.alternatives),
            "created_at": self.created_at,
            "updated_at": self.updated_at,
            "verification": {
                "question": self.verification_question,
                "expected_information_gain": self.expected_information_gain,
            },
            "provenance": {
                "module": self.provenance_module,
                "model_version": self.model_version,
            },
        }
        _validate_contract(value, "hypothesis.schema.json")
        return value


@dataclass(frozen=True)
class CuriosityConfig:
    """Local, reproducible calibration and interruption policy."""

    calibration_enabled: bool = True
    question_policy: QuestionPolicy = QuestionPolicy.information_gain_v1
    interruptions_per_window: int = 3
    interruption_window_seconds: float = 900.0
    cooldown_seconds: float = 300.0
    correction_cooldown_seconds: float = 1800.0
    min_information_gain: float = 0.1
    silence_confidence: float = 0.9
    redundancy_suppression_enabled: bool = True
    budget_enabled: bool = True
    cooldown_enabled: bool = True
    calibration_bucket_count: int = 10

    def __post_init__(self) -> None:
        try:
            object.__setattr__(self, "question_policy", QuestionPolicy(self.question_policy))
        except ValueError as error:
            raise MetacognitionCuriosityError("unsupported question policy") from error
        if self.interruptions_per_window <= 0:
            raise MetacognitionCuriosityError("interruptions_per_window must be positive")
        if self.calibration_bucket_count <= 1:
            raise MetacognitionCuriosityError("calibration_bucket_count must exceed one")
        for name in (
            "interruption_window_seconds",
            "cooldown_seconds",
            "correction_cooldown_seconds",
        ):
            value = getattr(self, name)
            if not math.isfinite(value) or value < 0:
                raise MetacognitionCuriosityError(f"{name} must be finite and non-negative")
        _probability(self.min_information_gain, "min_information_gain")
        _probability(self.silence_confidence, "silence_confidence")

    @property
    def fingerprint(self) -> str:
        encoded = json.dumps(
            {
                "calibration_enabled": self.calibration_enabled,
                "question_policy": self.question_policy.value,
                "interruptions_per_window": self.interruptions_per_window,
                "interruption_window_seconds": self.interruption_window_seconds,
                "cooldown_seconds": self.cooldown_seconds,
                "correction_cooldown_seconds": self.correction_cooldown_seconds,
                "min_information_gain": self.min_information_gain,
                "silence_confidence": self.silence_confidence,
                "redundancy_suppression_enabled": self.redundancy_suppression_enabled,
                "budget_enabled": self.budget_enabled,
                "cooldown_enabled": self.cooldown_enabled,
                "calibration_bucket_count": self.calibration_bucket_count,
            },
            sort_keys=True,
            separators=(",", ":"),
        )
        return hashlib.sha256(encoded.encode("utf-8")).hexdigest()[:16]

    def without_calibration(self) -> CuriosityConfig:
        return replace(self, calibration_enabled=False)


@dataclass(frozen=True)
class MetacognitiveAssessment:
    assessment_id: str
    schema_version: str
    hypothesis_id: str
    evaluated_at: str
    raw_confidence: float
    calibrated_confidence: float
    evidence_balance: float | None
    uncertainty: float
    supporting_refs: tuple[str, ...]
    opposing_refs: tuple[str, ...]
    alternatives: tuple[str, ...]
    decision: str
    reason_codes: tuple[str, ...]
    calibrator_id: str

    def to_mapping(self) -> dict[str, Any]:
        value = {
            "assessment_id": self.assessment_id,
            "schema_version": self.schema_version,
            "hypothesis_id": self.hypothesis_id,
            "evaluated_at": self.evaluated_at,
            "raw_confidence": self.raw_confidence,
            "calibrated_confidence": self.calibrated_confidence,
            "evidence_balance": self.evidence_balance,
            "uncertainty": self.uncertainty,
            "supporting_refs": list(self.supporting_refs),
            "opposing_refs": list(self.opposing_refs),
            "alternatives": list(self.alternatives),
            "decision": self.decision,
            "reason_codes": list(self.reason_codes),
            "calibrator_id": self.calibrator_id,
        }
        _validate_contract(value, "metacognitive_assessment.schema.json")
        return value


@dataclass(frozen=True)
class CuriosityQuestion:
    question_id: str
    schema_version: str
    hypothesis_id: str
    assessment_id: str
    prompt: str
    expected_information_gain: float
    created_at: str
    status: QuestionStatus
    suppression_reason: str | None
    budget_window_started_at: str
    cooldown_until: str | None
    correction_count: int
    provenance_module: str
    policy_id: str

    def to_mapping(self) -> dict[str, Any]:
        value = {
            "question_id": self.question_id,
            "schema_version": self.schema_version,
            "hypothesis_id": self.hypothesis_id,
            "assessment_id": self.assessment_id,
            "prompt": self.prompt,
            "expected_information_gain": self.expected_information_gain,
            "created_at": self.created_at,
            "status": self.status.value,
            "suppression_reason": self.suppression_reason,
            "budget_window_started_at": self.budget_window_started_at,
            "cooldown_until": self.cooldown_until,
            "correction_count": self.correction_count,
            "provenance": {
                "module": self.provenance_module,
                "policy_id": self.policy_id,
            },
        }
        _validate_contract(value, "curiosity_question.schema.json")
        return value


@dataclass(frozen=True)
class CuriosityResponse:
    response_id: str
    schema_version: str
    question_id: str
    received_at: str
    outcome: ResponseOutcome
    correction: bool
    evidence_refs: tuple[str, ...]
    source: str
    actor_id: str | None

    def to_mapping(self) -> dict[str, Any]:
        value = {
            "response_id": self.response_id,
            "schema_version": self.schema_version,
            "question_id": self.question_id,
            "received_at": self.received_at,
            "outcome": self.outcome.value,
            "correction": self.correction,
            "evidence_refs": list(self.evidence_refs),
            "provenance": {"source": self.source, "actor_id": self.actor_id},
        }
        _validate_contract(value, "curiosity_response.schema.json")
        return value


class MetacognitionCuriosityEngine:
    """Evaluate hypotheses and propose or suppress local structured questions."""

    def __init__(self, config: CuriosityConfig | None = None) -> None:
        self.config = config or CuriosityConfig()
        self._hypotheses: dict[str, HypothesisRecord] = {}
        self._assessments: dict[str, MetacognitiveAssessment] = {}
        self._questions: dict[str, CuriosityQuestion] = {}
        self._responses: list[CuriosityResponse] = []
        self._calibration_observations: list[tuple[float, float]] = []
        self._metric_outcomes: list[tuple[float, float]] = []
        self._asked_at: list[datetime] = []
        self._asked_fingerprints: set[tuple[str, str]] = set()
        self._cooldown_until: dict[str, datetime] = {}
        self._correction_count: dict[str, int] = {}

    def evaluate(
        self, hypothesis: HypothesisRecord, now: str | None = None
    ) -> MetacognitiveAssessment:
        """Create an auditable assessment without inventing evidence."""

        moment = _clock(now)
        self._hypotheses[hypothesis.hypothesis_id] = hypothesis
        evidence_total = len(hypothesis.supporting_refs) + len(hypothesis.opposing_refs)
        evidence_balance = (
            len(hypothesis.supporting_refs) / evidence_total if evidence_total else None
        )
        raw = (
            hypothesis.confidence
            if evidence_balance is None
            else (hypothesis.confidence + evidence_balance) / 2
        )
        calibrated = self._calibrate(raw)
        uncertainty = _binary_entropy(calibrated)
        decision = "silence" if calibrated >= self.config.silence_confidence else "question"
        reasons = [f"confidence.raw:{RAW_CONFIDENCE_ID}"]
        if evidence_balance is None:
            reasons.append("evidence.absent")
        else:
            reasons.append("evidence.balance_observed")
        reasons.append(f"calibration:{self._calibrator_identifier}")
        reasons.append(f"decision:{decision}")
        assessment_id = str(
            uuid.uuid5(
                _NAMESPACE,
                f"{hypothesis.hypothesis_id}:{_format_time(moment)}:{len(self._calibration_observations)}:{self.config.fingerprint}",
            )
        )
        assessment = MetacognitiveAssessment(
            assessment_id=assessment_id,
            schema_version=SCHEMA_VERSION,
            hypothesis_id=hypothesis.hypothesis_id,
            evaluated_at=_format_time(moment),
            raw_confidence=raw,
            calibrated_confidence=calibrated,
            evidence_balance=evidence_balance,
            uncertainty=uncertainty,
            supporting_refs=hypothesis.supporting_refs,
            opposing_refs=hypothesis.opposing_refs,
            alternatives=hypothesis.alternatives,
            decision=decision,
            reason_codes=tuple(reasons),
            calibrator_id=self._calibrator_identifier,
        )
        assessment.to_mapping()
        self._assessments[assessment_id] = assessment
        return assessment

    def propose_question(
        self,
        assessment_id: str,
        prompt: str,
        *,
        expected_resolution: float,
        now: str | None = None,
    ) -> CuriosityQuestion:
        """Propose or explicitly suppress a question according to local policy."""

        assessment = self._assessments.get(assessment_id)
        if assessment is None:
            raise MetacognitionCuriosityError("assessment is unavailable")
        _required_string(prompt, "prompt")
        _probability(expected_resolution, "expected_resolution")
        moment = _clock(now)
        fingerprint = _question_fingerprint(prompt)
        expected_gain = self._expected_gain(assessment, expected_resolution)
        hypothesis_id = assessment.hypothesis_id
        correction_count = self._correction_count.get(hypothesis_id, 0)
        suppression = self._suppression_reason(
            assessment,
            hypothesis_id,
            fingerprint,
            expected_gain,
            moment,
        )
        question_id = str(
            uuid.uuid5(
                _NAMESPACE,
                f"{assessment_id}:{fingerprint}:{_format_time(moment)}:{len(self._questions)}",
            )
        )
        cooldown = self._cooldown_until.get(hypothesis_id)
        question = CuriosityQuestion(
            question_id=question_id,
            schema_version=SCHEMA_VERSION,
            hypothesis_id=hypothesis_id,
            assessment_id=assessment_id,
            prompt=prompt,
            expected_information_gain=expected_gain,
            created_at=_format_time(moment),
            status=QuestionStatus.suppressed if suppression else QuestionStatus.proposed,
            suppression_reason=suppression,
            budget_window_started_at=_format_time(
                moment - timedelta(seconds=self.config.interruption_window_seconds)
            ),
            cooldown_until=_format_time(cooldown) if cooldown is not None else None,
            correction_count=correction_count,
            provenance_module=CREATED_BY,
            policy_id=self.config.question_policy.value,
        )
        question.to_mapping()
        self._questions[question_id] = question
        return question

    def ask(self, question_id: str, now: str | None = None) -> CuriosityQuestion:
        """Record an internal interruption; external delivery is outside this SPEC."""

        question = self._questions.get(question_id)
        if question is None:
            raise MetacognitionCuriosityError("question is unavailable")
        if question.status is not QuestionStatus.proposed:
            raise MetacognitionCuriosityError("only proposed questions can be asked")
        moment = _clock(now)
        if self.config.budget_enabled and not self._budget_available(moment):
            raise MetacognitionCuriosityError("interruption budget is exhausted")
        updated = replace(question, status=QuestionStatus.asked)
        updated.to_mapping()
        self._questions[question_id] = updated
        self._asked_at.append(moment)
        self._asked_fingerprints.add((question.hypothesis_id, _question_fingerprint(question.prompt)))
        if self.config.cooldown_enabled:
            self._cooldown_until[question.hypothesis_id] = moment + timedelta(
                seconds=self.config.cooldown_seconds
            )
        return updated

    def record_response(
        self,
        question_id: str,
        *,
        outcome: ResponseOutcome,
        correction: bool,
        evidence_refs: Sequence[str],
        source: str,
        actor_id: str | None,
        now: str | None = None,
    ) -> CuriosityResponse:
        """Record local feedback and update calibration or repetition controls."""

        question = self._questions.get(question_id)
        if question is None:
            raise MetacognitionCuriosityError("question is unavailable")
        if question.status is not QuestionStatus.asked:
            raise MetacognitionCuriosityError("only asked questions can receive a response")
        try:
            normalized_outcome = ResponseOutcome(outcome)
        except ValueError as error:
            raise MetacognitionCuriosityError("unsupported response outcome") from error
        references = _references(evidence_refs, "evidence_refs")
        _required_string(source, "source")
        if actor_id is not None:
            _required_string(actor_id, "actor_id")
        moment = _clock(now)
        response = CuriosityResponse(
            response_id=str(
                uuid.uuid5(
                    _NAMESPACE,
                    f"{question_id}:{_format_time(moment)}:{normalized_outcome.value}:{references}",
                )
            ),
            schema_version=SCHEMA_VERSION,
            question_id=question_id,
            received_at=_format_time(moment),
            outcome=normalized_outcome,
            correction=correction,
            evidence_refs=references,
            source=source,
            actor_id=actor_id,
        )
        response.to_mapping()
        self._questions[question_id] = replace(question, status=QuestionStatus.answered)
        self._responses.append(response)
        assessment = self._assessments[question.assessment_id]
        if normalized_outcome is not ResponseOutcome.inconclusive:
            verified_outcome = (
                1.0 if normalized_outcome is ResponseOutcome.confirmed else 0.0
            )
            self._calibration_observations.append(
                (assessment.raw_confidence, verified_outcome)
            )
            self._metric_outcomes.append(
                (assessment.calibrated_confidence, verified_outcome)
            )
        if correction:
            count = self._correction_count.get(question.hypothesis_id, 0) + 1
            self._correction_count[question.hypothesis_id] = count
            if self.config.cooldown_enabled:
                self._cooldown_until[question.hypothesis_id] = moment + timedelta(
                    seconds=self.config.correction_cooldown_seconds
                )
        return response

    def metrics(self) -> dict[str, Any]:
        """Return calibration and interruption metrics without scientific overclaim."""

        calibration = _calibration_metrics(
            self._metric_outcomes, self.config.calibration_bucket_count
        )
        return {
            "baseline_confidence_id": BASELINE_CONFIDENCE_ID,
            "baseline_question_policy_id": BASELINE_QUESTION_POLICY_ID,
            "confidence_policy_id": self._calibrator_identifier,
            "question_policy_id": self.config.question_policy.value,
            "registered": True,
            "hypothesis": HYPOTHESIS,
            "ablation": ABLATION,
            "falsification": FALSIFICATION,
            "calibration": calibration,
            "questions": [
                {
                    "question_id": question.question_id,
                    "status": question.status.value,
                    "expected_information_gain": question.expected_information_gain,
                    "suppression_reason": question.suppression_reason,
                }
                for question in sorted(self._questions.values(), key=lambda value: value.question_id)
            ],
            "responses": [response.to_mapping() for response in self._responses],
        }

    def snapshot(self) -> dict[str, Any]:
        """Return a deterministic local audit snapshot for replay experiments."""

        return {
            "schema_version": SCHEMA_VERSION,
            "config_fingerprint": self.config.fingerprint,
            "hypotheses": [
                value.to_mapping()
                for value in sorted(self._hypotheses.values(), key=lambda value: value.hypothesis_id)
            ],
            "assessments": [
                value.to_mapping()
                for value in sorted(self._assessments.values(), key=lambda value: value.assessment_id)
            ],
            "questions": [
                value.to_mapping()
                for value in sorted(self._questions.values(), key=lambda value: value.question_id)
            ],
            "responses": [value.to_mapping() for value in self._responses],
            "metrics": self.metrics(),
        }

    @property
    def _calibrator_identifier(self) -> str:
        return CALIBRATOR_ID if self.config.calibration_enabled else BASELINE_CONFIDENCE_ID

    def _calibrate(self, raw_confidence: float) -> float:
        if not self.config.calibration_enabled:
            return raw_confidence
        bucket = _bucket(raw_confidence, self.config.calibration_bucket_count)
        outcomes = [
            outcome
            for confidence, outcome in self._calibration_observations
            if _bucket(confidence, self.config.calibration_bucket_count) == bucket
        ]
        if not outcomes:
            return raw_confidence
        return (2 * raw_confidence + sum(outcomes)) / (2 + len(outcomes))

    def _expected_gain(
        self, assessment: MetacognitiveAssessment, expected_resolution: float
    ) -> float:
        if self.config.question_policy is QuestionPolicy.fixed_gain_v0:
            return 0.5
        correction_penalty = 1 + self._correction_count.get(assessment.hypothesis_id, 0)
        return (_binary_entropy(assessment.calibrated_confidence) * expected_resolution) / correction_penalty

    def _suppression_reason(
        self,
        assessment: MetacognitiveAssessment,
        hypothesis_id: str,
        fingerprint: str,
        expected_gain: float,
        moment: datetime,
    ) -> str | None:
        if self.config.redundancy_suppression_enabled and (hypothesis_id, fingerprint) in self._asked_fingerprints:
            return "redundant_question"
        cooldown = self._cooldown_until.get(hypothesis_id)
        if self.config.cooldown_enabled and cooldown is not None and moment < cooldown:
            return "correction_cooldown" if self._correction_count.get(hypothesis_id, 0) else "cooldown"
        if self.config.budget_enabled and not self._budget_available(moment):
            return "interruption_budget"
        if assessment.decision == "silence":
            return "sufficiently_calibrated"
        if expected_gain < self.config.min_information_gain:
            return "low_information_gain"
        return None

    def _budget_available(self, moment: datetime) -> bool:
        start = moment - timedelta(seconds=self.config.interruption_window_seconds)
        self._asked_at = [value for value in self._asked_at if value >= start]
        return len(self._asked_at) < self.config.interruptions_per_window


def _calibration_metrics(
    outcomes: Sequence[tuple[float, float]], bucket_count: int
) -> dict[str, Any]:
    if not outcomes:
        return {
            "outcome_count": 0,
            "brier": None,
            "ece": None,
            "auroc": None,
            "risk_coverage": [],
        }
    brier = sum((confidence - outcome) ** 2 for confidence, outcome in outcomes) / len(outcomes)
    grouped: dict[int, list[tuple[float, float]]] = {}
    for confidence, outcome in outcomes:
        grouped.setdefault(_bucket(confidence, bucket_count), []).append((confidence, outcome))
    ece = sum(
        (len(values) / len(outcomes))
        * abs(
            sum(confidence for confidence, _ in values) / len(values)
            - sum(outcome for _, outcome in values) / len(values)
        )
        for values in grouped.values()
    )
    ordered = sorted(outcomes, key=lambda value: value[0], reverse=True)
    correct = 0.0
    risk_coverage = []
    for index, (_, outcome) in enumerate(ordered, start=1):
        correct += outcome
        risk_coverage.append({"coverage": index / len(ordered), "risk": 1 - correct / index})
    return {
        "outcome_count": len(outcomes),
        "brier": brier,
        "ece": ece,
        "auroc": _auroc(outcomes),
        "risk_coverage": risk_coverage,
    }


def _auroc(outcomes: Sequence[tuple[float, float]]) -> float | None:
    positive = [confidence for confidence, outcome in outcomes if outcome == 1]
    negative = [confidence for confidence, outcome in outcomes if outcome == 0]
    if not positive or not negative:
        return None
    comparisons = [
        1.0 if first > second else 0.5 if first == second else 0.0
        for first in positive
        for second in negative
    ]
    return sum(comparisons) / len(comparisons)


def _validate_contract(value: Mapping[str, Any], schema_name: str) -> None:
    try:
        validate_shared_schema(value, schema_name)
    except ValueError as error:
        raise MetacognitionCuriosityError(str(error)) from error


def _required_string(value: str, name: str) -> None:
    if not isinstance(value, str) or not value.strip():
        raise MetacognitionCuriosityError(f"{name} must be a non-empty string")


def _references(values: Collection[str], name: str) -> tuple[str, ...]:
    if not isinstance(values, Collection) or isinstance(values, str):
        raise MetacognitionCuriosityError(f"{name} must be a collection of strings")
    normalized = tuple(values)
    if any(not isinstance(value, str) or not value.strip() for value in normalized):
        raise MetacognitionCuriosityError(f"{name} must contain non-empty strings")
    return normalized


def _probability(value: float, name: str) -> None:
    if isinstance(value, bool) or not math.isfinite(float(value)) or not 0 <= float(value) <= 1:
        raise MetacognitionCuriosityError(f"{name} must be a finite number between zero and one")


def _clock(value: str | None) -> datetime:
    return datetime.now(UTC) if value is None else _parse_time(value, "timestamp")


def _parse_time(value: str, name: str) -> datetime:
    if not isinstance(value, str):
        raise MetacognitionCuriosityError(f"{name} must be an ISO-8601 string")
    try:
        parsed = datetime.fromisoformat(value)
    except ValueError as error:
        raise MetacognitionCuriosityError(f"{name} must be a valid ISO-8601 timestamp") from error
    if parsed.tzinfo is None:
        raise MetacognitionCuriosityError(f"{name} must include a timezone")
    return parsed.astimezone(UTC)


def _format_time(value: datetime) -> str:
    return value.astimezone(UTC).isoformat()


def _bucket(confidence: float, bucket_count: int) -> int:
    return min(bucket_count - 1, int(confidence * bucket_count))


def _binary_entropy(confidence: float) -> float:
    if confidence <= 0 or confidence >= 1:
        return 0.0
    return -(confidence * math.log2(confidence) + (1 - confidence) * math.log2(1 - confidence))


def _question_fingerprint(prompt: str) -> str:
    return " ".join(prompt.casefold().split())
