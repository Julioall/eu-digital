"""Human annotation records for synthetic sessions."""

from __future__ import annotations

from typing import Any

from .sandbox import SyntheticSession


class AnnotationError(ValueError):
    """Raised when an annotation does not match the session ground truth."""


def annotate_session(
    session: SyntheticSession,
    annotations: dict[str, dict[str, Any]],
    annotator_id: str = "human",
) -> dict[str, Any]:
    episode_by_id = {episode.episode_id: episode for episode in session.episodes}
    unknown = set(annotations) - set(episode_by_id)
    if unknown:
        raise AnnotationError(f"unknown episode ids: {sorted(unknown)}")
    if not annotator_id.strip():
        raise AnnotationError("annotator_id cannot be empty")

    records: list[dict[str, Any]] = []
    for episode_id, values in annotations.items():
        label = values.get("label")
        if not isinstance(label, str) or not label.strip():
            raise AnnotationError(f"label is required for {episode_id}")
        relevance = values.get("relevance")
        if relevance is not None and (not isinstance(relevance, (int, float)) or not 0 <= relevance <= 1):
            raise AnnotationError(f"relevance must be between 0 and 1 for {episode_id}")
        episode = episode_by_id[episode_id]
        records.append(
            {
                "episode_id": episode_id,
                "event_ids": episode.event_ids,
                "label": label,
                "goal": values.get("goal"),
                "relevance": relevance,
            }
        )

    return {
        "schema_version": "1.0",
        "session_id": session.session_id,
        "annotator_id": annotator_id,
        "annotations": records,
    }


def calculate_agreement(first: dict[str, Any], second: dict[str, Any]) -> dict[str, Any]:
    """Calculate exact agreement and Cohen's kappa over shared episode labels."""

    first_labels = {item["episode_id"]: item["label"] for item in first.get("annotations", [])}
    second_labels = {item["episode_id"]: item["label"] for item in second.get("annotations", [])}
    shared = sorted(set(first_labels) & set(second_labels))
    if not shared:
        raise AnnotationError("at least one shared episode is required")

    exact = sum(first_labels[item] == second_labels[item] for item in shared) / len(shared)
    labels = sorted(set(first_labels[item] for item in shared) | set(second_labels[item] for item in shared))
    expected = sum(
        (sum(first_labels[item] == label for item in shared) / len(shared))
        * (sum(second_labels[item] == label for item in shared) / len(shared))
        for label in labels
    )
    kappa = 1.0 if expected == 1.0 else (exact - expected) / (1.0 - expected)
    return {
        "schema_version": "1.0",
        "session_id": first.get("session_id"),
        "annotators": [first.get("annotator_id"), second.get("annotator_id")],
        "compared_episodes": len(shared),
        "exact_agreement": exact,
        "cohen_kappa": kappa,
    }
