import ast
import sys
import unittest
from pathlib import Path

LAB_ROOT = Path(__file__).resolve().parents[1]
if str(LAB_ROOT) not in sys.path:
    sys.path.insert(0, str(LAB_ROOT))

from eu_digital_lab.dialogue_avatar import (
    ABLATION,
    FALSIFICATION,
    HYPOTHESIS,
    AvatarState,
    AvatarViewState,
    DialogueAvatarController,
    DialogueAvatarError,
    DialogueFeedback,
    DialogueNotice,
    FeedbackAction,
)

TIME_ZERO = "2026-07-29T12:00:00+00:00"


def notice(notice_id: str = "notice-1", kind: str = "question") -> DialogueNotice:
    return DialogueNotice(
        notice_id=notice_id,
        kind=kind,
        hypothesis_id="hypothesis-1",
        confidence=0.65,
        context_refs=("episode-1", "pattern-1"),
        reason="The observed pattern is uncertain.",
        question="Should this observation be corrected?",
        created_at=TIME_ZERO,
    )


class Presenter:
    def __init__(self) -> None:
        self.states: list[AvatarViewState] = []

    def publish(self, state: AvatarViewState) -> None:
        self.states.append(state)


class DialogueAvatarTests(unittest.TestCase):
    def test_question_contains_context_reason_and_confidence(self) -> None:
        current = DialogueAvatarController("avatar-1").present(notice())

        self.assertEqual(current.state, AvatarState.question)
        mapped = notice().to_mapping()
        self.assertEqual(mapped["context_refs"], ["episode-1", "pattern-1"])
        self.assertEqual(mapped["reason"], "The observed pattern is uncertain.")
        self.assertEqual(mapped["confidence"], 0.65)

    def test_view_is_non_blocking_and_never_captures_focus_or_input(self) -> None:
        presenter = Presenter()
        controller = DialogueAvatarController("avatar-1", presenter=presenter)
        view = controller.present(notice())

        self.assertFalse(view.blocks_work)
        self.assertFalse(view.captures_input)
        self.assertFalse(view.has_focus)
        self.assertEqual(presenter.states[-1], view)
        self.assertFalse(controller.metrics()["blocks_work"])

    def test_user_can_correct_defer_and_silence(self) -> None:
        controller = DialogueAvatarController("avatar-1", window_seconds=60)
        controller.present(notice("correct"))
        corrected = controller.record_feedback(
            DialogueFeedback("feedback-c", "correct", FeedbackAction.correct, TIME_ZERO, "The pattern is wrong.")
        )
        controller.present(notice("defer"), "2026-07-29T12:00:01+00:00")
        deferred = controller.record_feedback(
            DialogueFeedback("feedback-d", "defer", FeedbackAction.defer, "2026-07-29T12:00:01+00:00", None)
        )
        controller.present(notice("silence"), "2026-07-29T12:00:02+00:00")
        silenced = controller.record_feedback(
            DialogueFeedback("feedback-s", "silence", FeedbackAction.silence, "2026-07-29T12:00:02+00:00", None)
        )

        self.assertEqual(corrected.state, AvatarState.quiet)
        self.assertEqual(deferred.state, AvatarState.quiet)
        self.assertEqual(silenced.state, AvatarState.hidden)
        self.assertEqual(len(controller.history()), 3)

    def test_silenced_notice_and_budget_do_not_interrupt(self) -> None:
        controller = DialogueAvatarController("avatar-1", max_notices_per_window=1)
        controller.present(notice("first"))
        controller.record_feedback(
            DialogueFeedback("silence-first", "first", FeedbackAction.silence, TIME_ZERO, None)
        )
        hidden = controller.present(notice("first"), "2026-07-29T12:00:01+00:00")
        quiet = controller.present(notice("second"), "2026-07-29T12:00:02+00:00")

        self.assertEqual(hidden.state, AvatarState.hidden)
        self.assertEqual(quiet.state, AvatarState.quiet)
        self.assertIsNone(quiet.notice_id)

    def test_notifications_and_history_are_structured(self) -> None:
        controller = DialogueAvatarController("avatar-1")
        controller.present(notice("notification", "notification"))
        snapshot = controller.snapshot()

        self.assertEqual(snapshot["view"]["state"], "notice")
        self.assertEqual(snapshot["notices"][0]["kind"], "notification")
        self.assertEqual(snapshot["feedback"], [])

    def test_invalid_feedback_transition_is_typed(self) -> None:
        controller = DialogueAvatarController("avatar-1")
        with self.assertRaises(DialogueAvatarError):
            controller.record_feedback(
                DialogueFeedback("unknown", "missing", FeedbackAction.silence, TIME_ZERO, None)
            )
        with self.assertRaises(DialogueAvatarError):
            DialogueFeedback("bad", "notice-1", FeedbackAction.defer, TIME_ZERO, "not allowed")

    def test_replay_is_deterministic_and_has_no_external_ui_or_llm_import(self) -> None:
        first = DialogueAvatarController("avatar-1")
        second = DialogueAvatarController("avatar-1")
        for controller in (first, second):
            controller.present(notice())
            controller.record_feedback(
                DialogueFeedback("feedback-1", "notice-1", FeedbackAction.correct, TIME_ZERO, "correction")
            )
        source = (LAB_ROOT / "eu_digital_lab" / "dialogue_avatar.py").read_text(encoding="utf-8")
        tree = ast.parse(source)
        imports = [
            alias.name
            for node in ast.walk(tree)
            if isinstance(node, ast.Import)
            for alias in node.names
        ]
        imports.extend(node.module or "" for node in ast.walk(tree) if isinstance(node, ast.ImportFrom))

        self.assertEqual(first.snapshot(), second.snapshot())
        self.assertFalse(any(term in name.lower() for name in imports for term in ("llm", "tkinter", "qt", "tauri")))

    def test_scientific_metadata_is_registered(self) -> None:
        metrics = DialogueAvatarController("avatar-1").metrics()

        self.assertEqual(metrics["hypothesis"], HYPOTHESIS)
        self.assertEqual(metrics["ablation"], ABLATION)
        self.assertEqual(metrics["falsification"], FALSIFICATION)


if __name__ == "__main__":
    unittest.main()
