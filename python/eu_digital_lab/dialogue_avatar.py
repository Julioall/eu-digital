"""Non-blocking contextual dialogue and avatar presentation reference.

This module produces validated local view state. A desktop host is an injected
port; the controller never opens a window, captures input, or claims emotion.
"""

from __future__ import annotations

import uuid
from collections.abc import Collection, Mapping
from dataclasses import dataclass
from datetime import UTC, datetime, timedelta
from enum import Enum
from typing import Any, Protocol

from .schema_validation import validate_shared_schema

SCHEMA_VERSION = "1.0"
HYPOTHESIS = (
    "a non-blocking contextual presenter preserves useful question context and "
    "user control without increasing work interruption"
)
ABLATION = "disable presentation and retain the same notice/feedback history"
FALSIFICATION = (
    "the presenter captures focus/input, blocks work, or feedback cannot defer "
    "and silence a notice"
)
_NAMESPACE = uuid.UUID("4bf4b45d-ae32-4b91-a8b8-2fdd9d45fae4")


class DialogueAvatarError(ValueError):
    """Raised for invalid dialogue contracts or interaction transitions."""


class AvatarState(str, Enum):
    hidden = "hidden"
    quiet = "quiet"
    notice = "notice"
    question = "question"


class FeedbackAction(str, Enum):
    correct = "correct"
    defer = "defer"
    silence = "silence"


class AvatarPresenter(Protocol):
    """Optional host adapter; implementations must not capture work input."""

    def publish(self, state: AvatarViewState) -> None: ...


@dataclass(frozen=True)
class DialogueNotice:
    notice_id: str
    kind: str
    hypothesis_id: str
    confidence: float
    context_refs: tuple[str, ...]
    reason: str
    question: str
    created_at: str
    schema_version: str = SCHEMA_VERSION

    def __post_init__(self) -> None:
        _required(self.notice_id, "notice_id")
        if self.kind not in {"notification", "question"}:
            raise DialogueAvatarError("unsupported notice kind")
        _required(self.hypothesis_id, "hypothesis_id")
        _probability(self.confidence, "confidence")
        object.__setattr__(self, "context_refs", _refs(self.context_refs, "context_refs"))
        _required(self.reason, "reason")
        _required(self.question, "question")
        _time(self.created_at, "created_at")
        if self.schema_version != SCHEMA_VERSION:
            raise DialogueAvatarError("unsupported notice schema version")
        self.to_mapping()

    def to_mapping(self) -> dict[str, Any]:
        value = {
            "notice_id": self.notice_id,
            "schema_version": self.schema_version,
            "kind": self.kind,
            "hypothesis_id": self.hypothesis_id,
            "confidence": self.confidence,
            "context_refs": list(self.context_refs),
            "reason": self.reason,
            "question": self.question,
            "created_at": self.created_at,
        }
        _validate(value, "dialogue_notice.schema.json")
        return value


@dataclass(frozen=True)
class AvatarViewState:
    view_id: str
    state: AvatarState
    notice_id: str | None
    schema_version: str = SCHEMA_VERSION
    blocks_work: bool = False
    captures_input: bool = False
    has_focus: bool = False

    def to_mapping(self) -> dict[str, Any]:
        value = {
            "view_id": self.view_id,
            "schema_version": self.schema_version,
            "state": self.state.value,
            "blocks_work": self.blocks_work,
            "captures_input": self.captures_input,
            "has_focus": self.has_focus,
            "notice_id": self.notice_id,
        }
        _validate(value, "avatar_view_state.schema.json")
        return value


@dataclass(frozen=True)
class DialogueFeedback:
    feedback_id: str
    notice_id: str
    action: FeedbackAction
    occurred_at: str
    correction: str | None
    schema_version: str = SCHEMA_VERSION

    def __post_init__(self) -> None:
        _required(self.feedback_id, "feedback_id")
        _required(self.notice_id, "notice_id")
        try:
            object.__setattr__(self, "action", FeedbackAction(self.action))
        except ValueError as error:
            raise DialogueAvatarError("unsupported feedback action") from error
        _time(self.occurred_at, "occurred_at")
        if self.action is FeedbackAction.correct:
            _required(self.correction, "correction")
        elif self.correction is not None:
            raise DialogueAvatarError("only correct feedback accepts a correction")
        if self.schema_version != SCHEMA_VERSION:
            raise DialogueAvatarError("unsupported feedback schema version")
        self.to_mapping()

    def to_mapping(self) -> dict[str, Any]:
        value = {
            "feedback_id": self.feedback_id,
            "schema_version": self.schema_version,
            "notice_id": self.notice_id,
            "action": self.action.value,
            "occurred_at": self.occurred_at,
            "correction": self.correction,
        }
        _validate(value, "dialogue_feedback.schema.json")
        return value


class DialogueAvatarController:
    """Keep contextual notices and non-blocking presentation state local."""

    def __init__(
        self,
        view_id: str,
        *,
        presenter: AvatarPresenter | None = None,
        max_notices_per_window: int = 3,
        window_seconds: float = 900.0,
    ) -> None:
        _required(view_id, "view_id")
        if max_notices_per_window <= 0 or window_seconds < 0:
            raise DialogueAvatarError("invalid interruption budget")
        self.view_id = view_id
        self.presenter = presenter
        self.max_notices_per_window = max_notices_per_window
        self.window_seconds = window_seconds
        self._view = AvatarViewState(view_id, AvatarState.hidden, None)
        self._notices: dict[str, DialogueNotice] = {}
        self._feedback: list[DialogueFeedback] = []
        self._shown_at: list[datetime] = []
        self._silenced: set[str] = set()
        self._deferred_until: dict[str, datetime] = {}
        self._audit: list[dict[str, str]] = []

    @property
    def view(self) -> AvatarViewState:
        return self._view

    def present(self, notice: DialogueNotice, now: str | None = None) -> AvatarViewState:
        moment = _clock(now or notice.created_at)
        self._notices[notice.notice_id] = notice
        self._shown_at = [
            item for item in self._shown_at
            if item >= moment - timedelta(seconds=self.window_seconds)
        ]
        if notice.notice_id in self._silenced:
            return self._publish(AvatarState.hidden, None, "silenced")
        if notice.notice_id in self._deferred_until and moment < self._deferred_until[notice.notice_id]:
            return self._publish(AvatarState.quiet, None, "deferred")
        if len(self._shown_at) >= self.max_notices_per_window:
            return self._publish(AvatarState.quiet, None, "interruption_budget")
        self._shown_at.append(moment)
        state = AvatarState.question if notice.kind == "question" else AvatarState.notice
        return self._publish(state, notice.notice_id, "presented")

    def record_feedback(self, feedback: DialogueFeedback) -> AvatarViewState:
        if feedback.notice_id not in self._notices:
            raise DialogueAvatarError("feedback references an unknown notice")
        if any(item.feedback_id == feedback.feedback_id for item in self._feedback):
            raise DialogueAvatarError("feedback_id was already recorded")
        self._feedback.append(feedback)
        if feedback.action is FeedbackAction.silence:
            self._silenced.add(feedback.notice_id)
            return self._publish(AvatarState.hidden, None, "silenced")
        if feedback.action is FeedbackAction.defer:
            when = _time(feedback.occurred_at, "occurred_at") + timedelta(seconds=self.window_seconds)
            self._deferred_until[feedback.notice_id] = when
            return self._publish(AvatarState.quiet, None, "deferred")
        return self._publish(AvatarState.quiet, None, "corrected")

    def history(self) -> tuple[DialogueFeedback, ...]:
        return tuple(self._feedback)

    def metrics(self) -> dict[str, Any]:
        return {
            "hypothesis": HYPOTHESIS,
            "ablation": ABLATION,
            "falsification": FALSIFICATION,
            "presented_count": len(self._shown_at),
            "feedback_count": len(self._feedback),
            "silenced_count": len(self._silenced),
            "blocks_work": False,
            "captures_input": False,
            "has_focus": False,
        }

    def snapshot(self) -> dict[str, Any]:
        return {
            "view": self._view.to_mapping(),
            "notices": [item.to_mapping() for item in sorted(self._notices.values(), key=lambda item: item.notice_id)],
            "feedback": [item.to_mapping() for item in self._feedback],
            "metrics": self.metrics(),
        }

    def _publish(self, state: AvatarState, notice_id: str | None, reason: str) -> AvatarViewState:
        self._view = AvatarViewState(self.view_id, state, notice_id)
        self._audit.append({"event": reason, "notice_id": notice_id or ""})
        self._view.to_mapping()
        if self.presenter is not None:
            self.presenter.publish(self._view)
        return self._view


def _validate(value: Mapping[str, Any], schema_name: str) -> None:
    try:
        validate_shared_schema(value, schema_name)
    except ValueError as error:
        raise DialogueAvatarError(str(error)) from error


def _required(value: str | None, name: str) -> None:
    if not isinstance(value, str) or not value.strip():
        raise DialogueAvatarError(f"{name} must be a non-empty string")


def _refs(values: Collection[str], name: str) -> tuple[str, ...]:
    if isinstance(values, str):
        raise DialogueAvatarError(f"{name} must be a collection")
    normalized = tuple(values)
    if any(not isinstance(value, str) or not value.strip() for value in normalized):
        raise DialogueAvatarError(f"{name} must contain non-empty strings")
    return normalized


def _probability(value: float, name: str) -> None:
    if isinstance(value, bool) or not isinstance(value, (int, float)) or not 0 <= value <= 1:
        raise DialogueAvatarError(f"{name} must be between zero and one")


def _time(value: str, name: str) -> datetime:
    if not isinstance(value, str):
        raise DialogueAvatarError(f"{name} must be an ISO-8601 timestamp")
    try:
        parsed = datetime.fromisoformat(value)
    except ValueError as error:
        raise DialogueAvatarError(f"{name} must be a valid timestamp") from error
    if parsed.tzinfo is None:
        raise DialogueAvatarError(f"{name} must include a timezone")
    return parsed.astimezone(UTC)


def _clock(value: str) -> datetime:
    return _time(value, "timestamp")
