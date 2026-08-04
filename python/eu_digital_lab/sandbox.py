"""Synthetic routines and ground truth for reproducible research sessions."""

from __future__ import annotations

import json
import random
import uuid
from dataclasses import asdict, dataclass
from datetime import UTC, datetime, timedelta
from typing import Any

SCHEMA_VERSION = "1.0"
GENERATOR_VERSION = "1.0.0"
_NAMESPACE = uuid.UUID("9e7ac52e-93c4-4a85-8d31-b8e1b25c6d02")
_START = datetime(2026, 1, 1, tzinfo=UTC)


@dataclass(frozen=True)
class SyntheticEvent:
    event_id: str
    session_id: str
    sequence: int
    occurred_at: str
    source: str
    event_type: str
    payload: dict[str, Any]


@dataclass(frozen=True)
class Episode:
    episode_id: str
    session_id: str
    event_ids: list[str]
    routine: str
    boundary_reason: str


@dataclass(frozen=True)
class CausalLink:
    event_id: str
    actor: str
    cause: str


@dataclass(frozen=True)
class SyntheticSession:
    schema_version: str
    generator_version: str
    seed: int
    session_id: str
    events: list[SyntheticEvent]
    episodes: list[Episode]
    causal_links: list[CausalLink]


_ROUTINES: tuple[tuple[str, tuple[tuple[str, str, str], ...]], ...] = (
    (
        "open_editor",
        (
            ("system", "process_started", "editor"),
            ("user", "window_focused", "editor"),
            ("agent", "attention_selected", "editor"),
        ),
    ),
    (
        "edit_document",
        (
            ("user", "text_inserted", "document"),
            ("system", "file_changed", "document"),
            ("agent", "save_suggested", "document"),
        ),
    ),
    (
        "switch_window",
        (
            ("user", "window_blurred", "editor"),
            ("user", "window_focused", "browser"),
            ("agent", "context_recovered", "browser"),
        ),
    ),
    (
        "external_interrupt",
        (
            ("external", "notification_received", "calendar"),
            ("user", "notification_opened", "calendar"),
            ("agent", "question_prepared", "calendar"),
        ),
    ),
)


def _stable_id(kind: str, seed: int, index: int) -> str:
    return str(uuid.uuid5(_NAMESPACE, f"{kind}:{seed}:{index}"))


def generate_session(seed: int, routine_count: int = 4) -> SyntheticSession:
    """Generate a deterministic session with event, episode and causal truth."""

    if routine_count < 1:
        raise ValueError("routine_count must be positive")

    rng = random.Random(seed)
    session_id = _stable_id("session", seed, routine_count)
    routine_indexes = [rng.randrange(len(_ROUTINES)) for _ in range(routine_count)]
    events: list[SyntheticEvent] = []
    episodes: list[Episode] = []
    causal_links: list[CausalLink] = []
    sequence = 0

    for episode_index, routine_index in enumerate(routine_indexes):
        routine, event_specs = _ROUTINES[routine_index]
        episode_event_ids: list[str] = []
        episode_id = _stable_id("episode", seed, episode_index)
        for local_index, (source, event_type, subject) in enumerate(event_specs):
            event_id = _stable_id("event", seed, sequence)
            event = SyntheticEvent(
                event_id=event_id,
                session_id=session_id,
                sequence=sequence,
                occurred_at=(_START + timedelta(seconds=sequence)).isoformat().replace("+00:00", "Z"),
                source=source,
                event_type=event_type,
                payload={"subject": subject, "routine": routine, "local_index": local_index},
            )
            events.append(event)
            episode_event_ids.append(event_id)
            causal_links.append(
                CausalLink(
                    event_id=event_id,
                    actor=source if source in {"agent", "user", "external"} else "none",
                    cause=f"{routine}:{local_index}",
                )
            )
            sequence += 1
        episodes.append(
            Episode(
                episode_id=episode_id,
                session_id=session_id,
                event_ids=episode_event_ids,
                routine=routine,
                boundary_reason="routine_transition",
            )
        )

    return SyntheticSession(
        schema_version=SCHEMA_VERSION,
        generator_version=GENERATOR_VERSION,
        seed=seed,
        session_id=session_id,
        events=events,
        episodes=episodes,
        causal_links=causal_links,
    )


def session_to_dict(session: SyntheticSession) -> dict[str, Any]:
    result = asdict(session)
    result["episode_ids"] = [episode.episode_id for episode in session.episodes]
    return result


def session_from_dict(value: dict[str, Any]) -> SyntheticSession:
    return SyntheticSession(
        schema_version=str(value["schema_version"]),
        generator_version=str(value["generator_version"]),
        seed=int(value["seed"]),
        session_id=str(value["session_id"]),
        events=[SyntheticEvent(**event) for event in value["events"]],
        episodes=[Episode(**episode) for episode in value["episodes"]],
        causal_links=[CausalLink(**link) for link in value["causal_links"]],
    )


def write_session(session: SyntheticSession, path: str) -> None:
    with open(path, "w", encoding="utf-8", newline="\n") as stream:
        json.dump(session_to_dict(session), stream, ensure_ascii=False, indent=2, sort_keys=True)
        stream.write("\n")


def read_session(path: str) -> SyntheticSession:
    with open(path, encoding="utf-8") as stream:
        return session_from_dict(json.load(stream))


def split_sessions(
    sessions: list[SyntheticSession], seed: int, ratios: tuple[float, float, float] = (0.6, 0.2, 0.2)
) -> dict[str, list[SyntheticSession]]:
    """Split sessions deterministically into train, development and test."""

    if len(sessions) < 3:
        raise ValueError("at least three sessions are required for three splits")
    if len(ratios) != 3 or abs(sum(ratios) - 1.0) > 1e-9:
        raise ValueError("ratios must contain three values summing to one")

    ordered = list(sessions)
    random.Random(seed).shuffle(ordered)
    test_count = max(1, round(len(ordered) * ratios[2]))
    development_count = max(1, round(len(ordered) * ratios[1]))
    train_count = len(ordered) - test_count - development_count
    if train_count < 1:
        raise ValueError("ratios leave no training session")
    return {
        "train": ordered[:train_count],
        "development": ordered[train_count : train_count + development_count],
        "test": ordered[train_count + development_count :],
    }
