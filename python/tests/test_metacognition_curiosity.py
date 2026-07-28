import ast
import sys
import unittest
from pathlib import Path

LAB_ROOT = Path(__file__).resolve().parents[1]
if str(LAB_ROOT) not in sys.path:
    sys.path.insert(0, str(LAB_ROOT))

from eu_digital_lab.metacognition_curiosity import (
    ABLATION,
    BASELINE_CONFIDENCE_ID,
    BASELINE_QUESTION_POLICY_ID,
    CALIBRATOR_ID,
    FALSIFICATION,
    HYPOTHESIS,
    CuriosityConfig,
    HypothesisRecord,
    HypothesisStatus,
    MetacognitionCuriosityEngine,
    MetacognitionCuriosityError,
    QuestionPolicy,
    QuestionStatus,
    ResponseOutcome,
)

TIME_ZERO = "2026-07-28T12:00:00+00:00"


def hypothesis(
    hypothesis_id: str,
    *,
    confidence: float = 0.5,
    supporting_refs: tuple[str, ...] = ("pattern-1",),
    opposing_refs: tuple[str, ...] = ("counterexample-1",),
) -> HypothesisRecord:
    return HypothesisRecord(
        hypothesis_id=hypothesis_id,
        kind="predictive",
        statement=f"hypothesis {hypothesis_id}",
        status=HypothesisStatus.proposed,
        confidence=confidence,
        supporting_refs=supporting_refs,
        opposing_refs=opposing_refs,
        alternatives=("alternative-a",),
        created_at=TIME_ZERO,
        updated_at=TIME_ZERO,
        verification_question=None,
        expected_information_gain=None,
        provenance_module="test",
        model_version=None,
    )


class MetacognitionCuriosityTests(unittest.TestCase):
    def engine(self, config: CuriosityConfig | None = None) -> MetacognitionCuriosityEngine:
        return MetacognitionCuriosityEngine(config=config)

    def test_hypothesis_contract_preserves_evidence_and_alternatives(self) -> None:
        value = hypothesis("h-1")

        mapped = value.to_mapping()

        self.assertEqual(mapped["evidence"]["supporting_refs"], ["pattern-1"])
        self.assertEqual(mapped["evidence"]["opposing_refs"], ["counterexample-1"])
        self.assertEqual(mapped["alternatives"], ["alternative-a"])

    def test_rejected_outcome_calibrates_future_confidence(self) -> None:
        engine = self.engine(CuriosityConfig(silence_confidence=1.0))
        record = hypothesis("h-calibration", confidence=0.9, opposing_refs=())
        initial = engine.evaluate(record, TIME_ZERO)
        question = engine.propose_question(
            initial.assessment_id,
            "Did the expected transition happen?",
            expected_resolution=1.0,
            now=TIME_ZERO,
        )
        engine.ask(question.question_id, TIME_ZERO)
        engine.record_response(
            question.question_id,
            outcome=ResponseOutcome.rejected,
            correction=True,
            evidence_refs=("outcome-1",),
            source="human_annotation",
            actor_id="annotator-1",
            now="2026-07-28T12:00:01+00:00",
        )

        recalibrated = engine.evaluate(record, "2026-07-28T12:00:02+00:00")

        self.assertLess(recalibrated.calibrated_confidence, initial.calibrated_confidence)
        self.assertEqual(recalibrated.calibrator_id, CALIBRATOR_ID)

    def test_calibrator_buckets_verified_outcomes_by_raw_confidence(self) -> None:
        engine = self.engine(
            CuriosityConfig(cooldown_seconds=0, silence_confidence=1.0)
        )
        record = hypothesis(
            "h-raw-bucket",
            confidence=0.9,
            supporting_refs=(),
            opposing_refs=(),
        )
        first = engine.evaluate(record, TIME_ZERO)
        first_question = engine.propose_question(
            first.assessment_id,
            "Did prediction one occur?",
            expected_resolution=1.0,
            now=TIME_ZERO,
        )
        engine.ask(first_question.question_id, TIME_ZERO)
        engine.record_response(
            first_question.question_id,
            outcome=ResponseOutcome.rejected,
            correction=False,
            evidence_refs=("outcome-1",),
            source="fixture",
            actor_id=None,
            now="2026-07-28T12:00:01+00:00",
        )
        second = engine.evaluate(record, "2026-07-28T12:00:02+00:00")
        second_question = engine.propose_question(
            second.assessment_id,
            "Did prediction two occur?",
            expected_resolution=1.0,
            now="2026-07-28T12:00:02+00:00",
        )
        engine.ask(second_question.question_id, "2026-07-28T12:00:02+00:00")
        engine.record_response(
            second_question.question_id,
            outcome=ResponseOutcome.confirmed,
            correction=False,
            evidence_refs=("outcome-2",),
            source="fixture",
            actor_id=None,
            now="2026-07-28T12:00:03+00:00",
        )

        recalibrated = engine.evaluate(record, "2026-07-28T12:00:04+00:00")

        self.assertAlmostEqual(recalibrated.raw_confidence, 0.9)
        self.assertAlmostEqual(recalibrated.calibrated_confidence, 0.7)
        self.assertEqual(engine.metrics()["calibration"]["outcome_count"], 2)

    def test_every_question_references_an_assessment_hypothesis_and_gain(self) -> None:
        engine = self.engine(CuriosityConfig(silence_confidence=1.0))
        assessment = engine.evaluate(hypothesis("h-question"), TIME_ZERO)

        question = engine.propose_question(
            assessment.assessment_id,
            "Which observation would distinguish the alternatives?",
            expected_resolution=0.8,
            now=TIME_ZERO,
        )

        self.assertEqual(question.hypothesis_id, "h-question")
        self.assertEqual(question.assessment_id, assessment.assessment_id)
        self.assertGreater(question.expected_information_gain, 0.0)
        self.assertLessEqual(question.expected_information_gain, 1.0)
        self.assertEqual(question.status, QuestionStatus.proposed)

    def test_budget_redundancy_and_correction_reduce_repetition(self) -> None:
        config = CuriosityConfig(
            interruptions_per_window=1,
            interruption_window_seconds=10,
            cooldown_seconds=0,
            correction_cooldown_seconds=30,
            silence_confidence=1.0,
        )
        engine = self.engine(config)
        assessment = engine.evaluate(hypothesis("h-repeat"), TIME_ZERO)
        asked = engine.propose_question(
            assessment.assessment_id,
            "Can the observed pattern be confirmed?",
            expected_resolution=1.0,
            now=TIME_ZERO,
        )
        engine.ask(asked.question_id, TIME_ZERO)

        duplicate = engine.propose_question(
            assessment.assessment_id,
            "Can the observed pattern be confirmed?",
            expected_resolution=1.0,
            now="2026-07-28T12:00:01+00:00",
        )
        engine.record_response(
            asked.question_id,
            outcome=ResponseOutcome.rejected,
            correction=True,
            evidence_refs=("correction-1",),
            source="human_annotation",
            actor_id="annotator-1",
            now="2026-07-28T12:00:02+00:00",
        )
        corrected = engine.propose_question(
            assessment.assessment_id,
            "What evidence would overturn the prediction?",
            expected_resolution=1.0,
            now="2026-07-28T12:00:03+00:00",
        )

        self.assertEqual(duplicate.status, QuestionStatus.suppressed)
        self.assertEqual(duplicate.suppression_reason, "redundant_question")
        self.assertEqual(corrected.status, QuestionStatus.suppressed)
        self.assertEqual(corrected.suppression_reason, "correction_cooldown")

    def test_budget_can_suppress_a_non_redundant_question(self) -> None:
        engine = self.engine(
            CuriosityConfig(interruptions_per_window=1, silence_confidence=1.0)
        )
        first_assessment = engine.evaluate(hypothesis("h-budget-a"), TIME_ZERO)
        first = engine.propose_question(
            first_assessment.assessment_id,
            "Question A?",
            expected_resolution=1.0,
            now=TIME_ZERO,
        )
        engine.ask(first.question_id, TIME_ZERO)
        second_assessment = engine.evaluate(hypothesis("h-budget-b"), TIME_ZERO)

        suppressed = engine.propose_question(
            second_assessment.assessment_id,
            "Question B?",
            expected_resolution=1.0,
            now="2026-07-28T12:00:01+00:00",
        )

        self.assertEqual(suppressed.status, QuestionStatus.suppressed)
        self.assertEqual(suppressed.suppression_reason, "interruption_budget")

    def test_confident_hypothesis_can_remain_silent(self) -> None:
        engine = self.engine(CuriosityConfig(silence_confidence=0.95))
        assessment = engine.evaluate(
            hypothesis("h-silent", confidence=0.99, opposing_refs=()),
            TIME_ZERO,
        )

        question = engine.propose_question(
            assessment.assessment_id,
            "Should this be asked?",
            expected_resolution=1.0,
            now=TIME_ZERO,
        )

        self.assertEqual(assessment.decision, "silence")
        self.assertEqual(question.status, QuestionStatus.suppressed)
        self.assertEqual(question.suppression_reason, "sufficiently_calibrated")

    def test_inconclusive_response_is_not_negative_evidence(self) -> None:
        engine = self.engine(CuriosityConfig(silence_confidence=1.0))
        assessment = engine.evaluate(hypothesis("h-inconclusive"), TIME_ZERO)
        question = engine.propose_question(
            assessment.assessment_id,
            "Was the event observed?",
            expected_resolution=1.0,
            now=TIME_ZERO,
        )
        engine.ask(question.question_id, TIME_ZERO)
        engine.record_response(
            question.question_id,
            outcome=ResponseOutcome.inconclusive,
            correction=False,
            evidence_refs=("missing-observation",),
            source="human_annotation",
            actor_id="annotator-1",
            now="2026-07-28T12:00:01+00:00",
        )

        metrics = engine.metrics()

        self.assertEqual(metrics["calibration"]["outcome_count"], 0)
        self.assertEqual(metrics["responses"][0]["outcome"], "inconclusive")

    def test_ablation_and_scientific_metrics_are_registered(self) -> None:
        engine = self.engine(
            CuriosityConfig(
                calibration_enabled=False,
                question_policy=QuestionPolicy.fixed_gain_v0,
                silence_confidence=1.0,
            )
        )
        assessment = engine.evaluate(hypothesis("h-ablation", confidence=0.7), TIME_ZERO)
        question = engine.propose_question(
            assessment.assessment_id,
            "Fixed policy question?",
            expected_resolution=0.1,
            now=TIME_ZERO,
        )
        metrics = engine.metrics()

        self.assertEqual(assessment.raw_confidence, assessment.calibrated_confidence)
        self.assertEqual(question.expected_information_gain, 0.5)
        self.assertEqual(metrics["baseline_confidence_id"], BASELINE_CONFIDENCE_ID)
        self.assertEqual(metrics["baseline_question_policy_id"], BASELINE_QUESTION_POLICY_ID)
        self.assertEqual(metrics["ablation"], ABLATION)
        self.assertEqual(metrics["hypothesis"], HYPOTHESIS)
        self.assertEqual(metrics["falsification"], FALSIFICATION)

    def test_same_replay_is_deterministic_and_has_no_llm_dependency(self) -> None:
        config = CuriosityConfig(silence_confidence=1.0)
        first = self.engine(config)
        second = self.engine(config)
        record = hypothesis("h-replay")
        for engine in (first, second):
            assessment = engine.evaluate(record, TIME_ZERO)
            question = engine.propose_question(
                assessment.assessment_id,
                "Replay question?",
                expected_resolution=0.7,
                now=TIME_ZERO,
            )
            engine.ask(question.question_id, TIME_ZERO)
            engine.record_response(
                question.question_id,
                outcome=ResponseOutcome.confirmed,
                correction=False,
                evidence_refs=("replay-outcome",),
                source="fixture",
                actor_id=None,
                now="2026-07-28T12:00:01+00:00",
            )
        source = (LAB_ROOT / "eu_digital_lab" / "metacognition_curiosity.py").read_text(
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

    def test_invalid_transitions_are_typed_errors(self) -> None:
        engine = self.engine()
        with self.assertRaises(MetacognitionCuriosityError):
            engine.ask("missing", TIME_ZERO)
        with self.assertRaises(MetacognitionCuriosityError):
            hypothesis("h-invalid", confidence=1.2)


if __name__ == "__main__":
    unittest.main()
