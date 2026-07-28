"""Local episodic memory with explicit provenance and conservative retrieval.

The implementation stores episodes as observed records. It does not turn an
episode hypothesis into a fact, generate semantic summaries, or require an
embedding model. An optional local embedding provider can add a similarity
signal without changing the shared Episode contract.
"""

from __future__ import annotations

import json
import math
from collections.abc import Callable, Mapping, Sequence
from dataclasses import dataclass
from datetime import datetime
from enum import Enum
from itertools import combinations
from pathlib import Path
from typing import Any

from .schema_validation import validate_shared_schema

MEMORY_SCHEMA_VERSION = "1.0"
EmbeddingProvider = Callable[[Mapping[str, Any]], Sequence[float]]


class EpisodicMemoryError(ValueError):
    """Raised for invalid memory records or retrieval requests."""


class StoreResult(str, Enum):
    accepted = "accepted"
    duplicate = "duplicate"


@dataclass(frozen=True)
class MemoryQuery:
    session_id: str | None = None
    applications: tuple[str, ...] = ()
    documents: tuple[str, ...] = ()
    modalities: tuple[str, ...] = ()
    start_at: str | None = None
    end_at: str | None = None
    embedding: tuple[float, ...] | None = None
    limit: int = 10

    def __post_init__(self) -> None:
        if self.limit <= 0:
            raise ValueError("memory query limit must be positive")
        for value in (self.start_at, self.end_at):
            if value is not None:
                _parse_time(value)
        if self.embedding is not None:
            _validate_vector(self.embedding)


@dataclass(frozen=True)
class RetrievalResult:
    episode: dict[str, Any]
    score: float
    reason_codes: tuple[str, ...]
    explanation: str
    provenance: dict[str, Any]


class EpisodicMemory:
    """A deterministic local store for immutable Episode contract records."""

    def __init__(
        self,
        path: str | Path | None = None,
        *,
        embedding_provider: EmbeddingProvider | None = None,
        max_episodes: int = 10_000,
    ) -> None:
        if max_episodes <= 0:
            raise ValueError("max_episodes must be positive")
        self._path = Path(path) if path is not None else None
        self._embedding_provider = embedding_provider
        self._max_episodes = max_episodes
        self._episodes: dict[str, dict[str, Any]] = {}
        self._embeddings: dict[str, tuple[float, ...]] = {}
        if self._path is not None and self._path.exists():
            self._load()

    def size(self) -> int:
        return len(self._episodes)

    def store(self, episode: Mapping[str, Any]) -> StoreResult:
        value = _copy_episode(episode)
        episode_id = value["episode_id"]
        if episode_id in self._episodes:
            return StoreResult.duplicate
        self._episodes[episode_id] = value
        if self._embedding_provider is not None:
            try:
                vector = tuple(float(item) for item in self._embedding_provider(value))
                _validate_vector(vector)
            except (TypeError, ValueError):
                vector = ()
            if vector:
                self._embeddings[episode_id] = vector
        self._persist()
        return StoreResult.accepted

    def retrieve(self, query: MemoryQuery) -> list[RetrievalResult]:
        candidates: list[RetrievalResult] = []
        for episode_id, episode in self._episodes.items():
            result = self._match(episode_id, episode, query)
            if result is not None:
                candidates.append(result)
        candidates.sort(key=lambda item: (-item.score, item.episode["start_at"], item.episode["episode_id"]))
        return candidates[: query.limit]

    def similarity_relations(self, minimum_score: float = 0.0) -> list[dict[str, Any]]:
        """Return explicit, evidence-backed pair relations without generalizing."""

        if not 0.0 <= minimum_score <= 1.0:
            raise ValueError("minimum_score must be between zero and one")
        relations: list[dict[str, Any]] = []
        for left_id, right_id in combinations(sorted(self._episodes), 2):
            left = self._episodes[left_id]
            right = self._episodes[right_id]
            reasons: list[str] = []
            score = 0.0
            for field, reason in (
                ("applications", "context.application"),
                ("documents", "context.document"),
                ("modalities", "context.modality"),
            ):
                if set(left["context_summary"][field]) & set(right["context_summary"][field]):
                    reasons.append(reason)
                    score += 1.0 / 3.0
            left_embedding = self._embeddings.get(left_id)
            right_embedding = self._embeddings.get(right_id)
            if left_embedding is not None and right_embedding is not None and len(left_embedding) == len(right_embedding):
                score = max(score, max(0.0, _cosine(left_embedding, right_embedding)))
                if score > 0 and "embedding.cosine" not in reasons:
                    reasons.append("embedding.cosine")
            if score >= minimum_score and reasons:
                relations.append(
                    {
                        "episode_a": left_id,
                        "episode_b": right_id,
                        "score": score,
                        "reason_codes": reasons,
                        "provenance": {
                            "event_ids": [*left["event_ids"], *right["event_ids"]],
                        },
                    }
                )
        relations.sort(key=lambda item: (-item["score"], item["episode_a"], item["episode_b"]))
        return relations

    def consolidate(self) -> list[str]:
        """Apply bounded retention; no semantic generalization is performed."""

        if len(self._episodes) <= self._max_episodes:
            return []
        ordered = sorted(
            self._episodes,
            key=lambda episode_id: (
                self._episodes[episode_id]["end_at"],
                episode_id,
            ),
            reverse=True,
        )
        retained = set(ordered[: self._max_episodes])
        removed = sorted(episode_id for episode_id in self._episodes if episode_id not in retained)
        for episode_id in removed:
            del self._episodes[episode_id]
            self._embeddings.pop(episode_id, None)
        self._persist()
        return removed

    def _match(
        self, episode_id: str, episode: dict[str, Any], query: MemoryQuery
    ) -> RetrievalResult | None:
        context = episode["context_summary"]
        reasons: list[str] = []
        score = 0.0
        if query.session_id is not None:
            if episode["session_id"] != query.session_id:
                return None
            reasons.append("session.match")
            score += 0.1
        if query.applications:
            if not set(query.applications) & set(context["applications"]):
                return None
            reasons.append("context.application")
            score += 0.35
        if query.documents:
            if not set(query.documents) & set(context["documents"]):
                return None
            reasons.append("context.document")
            score += 0.35
        if query.modalities:
            if not set(query.modalities) & set(context["modalities"]):
                return None
            reasons.append("context.modality")
            score += 0.15
        if query.start_at is not None or query.end_at is not None:
            episode_start = _parse_time(episode["start_at"])
            episode_end = _parse_time(episode["end_at"])
            if query.end_at is not None and episode_start > _parse_time(query.end_at):
                return None
            if query.start_at is not None and episode_end < _parse_time(query.start_at):
                return None
            reasons.append("temporal.overlap")
            score += 0.05
        if query.embedding is not None:
            vector = self._embeddings.get(episode_id)
            if vector is None or len(vector) != len(query.embedding):
                return None
            similarity = _cosine(vector, query.embedding)
            if similarity <= 0:
                return None
            reasons.append("embedding.cosine")
            score += similarity
        if not reasons:
            reasons.append("chronological.fallback")
        provenance = {
            "episode_id": episode["episode_id"],
            "event_ids": list(episode["event_ids"]),
            "created_by": episode["created_by"],
            "schema_version": episode["schema_version"],
        }
        explanation = f"episode {episode_id} retrieved because " + ", ".join(reasons)
        return RetrievalResult(episode, score, tuple(reasons), explanation, provenance)

    def _load(self) -> None:
        assert self._path is not None
        try:
            envelope = json.loads(self._path.read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError) as error:
            raise EpisodicMemoryError(f"cannot load episodic memory: {self._path}") from error
        if not isinstance(envelope, dict) or envelope.get("schema_version") != MEMORY_SCHEMA_VERSION:
            raise EpisodicMemoryError("unsupported episodic memory schema")
        records = envelope.get("records")
        if not isinstance(records, list):
            raise EpisodicMemoryError("episodic memory records must be a list")
        for record in records:
            if not isinstance(record, dict) or not isinstance(record.get("episode"), dict):
                raise EpisodicMemoryError("invalid episodic memory record")
            episode = _copy_episode(record["episode"])
            episode_id = episode["episode_id"]
            if episode_id in self._episodes:
                raise EpisodicMemoryError(f"duplicate episode in memory: {episode_id}")
            self._episodes[episode_id] = episode
            embedding = record.get("embedding")
            if embedding is not None:
                if not isinstance(embedding, list):
                    raise EpisodicMemoryError("memory embedding must be an array or null")
                vector = tuple(float(item) for item in embedding)
                _validate_vector(vector)
                self._embeddings[episode_id] = vector

    def _persist(self) -> None:
        if self._path is None:
            return
        self._path.parent.mkdir(parents=True, exist_ok=True)
        envelope = {
            "schema_version": MEMORY_SCHEMA_VERSION,
            "records": [
                {"episode": self._episodes[episode_id], "embedding": list(self._embeddings[episode_id]) if episode_id in self._embeddings else None}
                for episode_id in sorted(self._episodes)
            ],
        }
        temporary = self._path.with_suffix(self._path.suffix + ".tmp")
        temporary.write_text(json.dumps(envelope, ensure_ascii=False, sort_keys=True, indent=2) + "\n", encoding="utf-8")
        temporary.replace(self._path)


def _copy_episode(episode: Mapping[str, Any]) -> dict[str, Any]:
    if not isinstance(episode, Mapping):
        raise EpisodicMemoryError("episode must be a mapping")
    value = json.loads(json.dumps(dict(episode), ensure_ascii=False, sort_keys=True))
    if not isinstance(value, dict):
        raise EpisodicMemoryError("episode must be an object")
    try:
        validate_shared_schema(value, "episode.schema.json")
    except ValueError as error:
        raise EpisodicMemoryError(str(error)) from error
    return value


def _parse_time(value: str) -> datetime:
    try:
        parsed = datetime.fromisoformat(value)
    except ValueError as error:
        raise ValueError(f"invalid episode timestamp: {value}") from error
    if parsed.tzinfo is None:
        raise ValueError("episode timestamps must include timezone")
    return parsed


def _validate_vector(vector: Sequence[float]) -> None:
    if not vector or not all(math.isfinite(float(item)) for item in vector):
        raise ValueError("embedding must contain finite values")


def _cosine(first: Sequence[float], second: Sequence[float]) -> float:
    numerator = sum(left * right for left, right in zip(first, second))
    first_norm = math.sqrt(sum(value * value for value in first))
    second_norm = math.sqrt(sum(value * value for value in second))
    if first_norm == 0 or second_norm == 0:
        return 0.0
    return numerator / (first_norm * second_norm)
