import json
import sys
import tempfile
import unittest
from pathlib import Path

LAB_ROOT = Path(__file__).resolve().parents[1]
if str(LAB_ROOT) not in sys.path:
    sys.path.insert(0, str(LAB_ROOT))

from eu_digital_lab.episodic_memory import (
    EpisodicMemory,
    MemoryQuery,
    StoreResult,
)


def episode(
    episode_id: str,
    application: str,
    document: str,
    *,
    start_at: str = "2026-01-01T00:00:00Z",
    hypotheses: list[str] | None = None,
) -> dict[str, object]:
    return {
        "episode_id": episode_id,
        "schema_version": "1.0",
        "session_id": "session-1",
        "start_at": start_at,
        "end_at": "2026-01-01T00:00:10Z",
        "event_ids": [f"{episode_id}-event"],
        "context_summary": {
            "applications": [application],
            "documents": [document],
            "people": [],
            "topics": [],
            "modalities": ["system"],
        },
        "boundary_reasons": ["episode_start"],
        "embedding_ref": None,
        "summary": None,
        "hypotheses": hypotheses or [],
        "quality": {"coherence": 1.0, "confidence": 1.0},
        "created_by": "test",
    }


class EpisodicMemoryTests(unittest.TestCase):
    def test_context_retrieval_explains_match_and_preserves_provenance(self) -> None:
        memory = EpisodicMemory()
        first = episode("episode-1", "editor", "a.txt")
        second = episode("episode-2", "browser", "b.html", start_at="2026-01-02T00:00:00Z")
        self.assertEqual(memory.store(first), StoreResult.accepted)
        self.assertEqual(memory.store(second), StoreResult.accepted)

        results = memory.retrieve(MemoryQuery(applications=("editor",)))

        self.assertEqual([item.episode["episode_id"] for item in results], ["episode-1"])
        self.assertIn("context.application", results[0].reason_codes)
        self.assertEqual(results[0].provenance["event_ids"], ["episode-1-event"])
        self.assertIn("episode-1", results[0].explanation)

    def test_local_embedding_is_optional_and_similarity_is_explainable(self) -> None:
        def embed(value: dict[str, object]) -> list[float]:
            applications = value["context_summary"]["applications"]  # type: ignore[index]
            return [1.0, 0.0] if applications == ["editor"] else [0.0, 1.0]

        memory = EpisodicMemory(embedding_provider=embed)
        first = episode("episode-1", "editor", "a.txt")
        second = episode("episode-2", "browser", "b.html")
        memory.store(first)
        memory.store(second)

        results = memory.retrieve(MemoryQuery(embedding=(0.9, 0.1)))

        self.assertEqual(results[0].episode["episode_id"], "episode-1")
        self.assertIn("embedding.cosine", results[0].reason_codes)
        self.assertNotIn("vector", results[0].episode)

    def test_hypotheses_are_not_returned_as_facts(self) -> None:
        memory = EpisodicMemory()
        stored = episode("episode-1", "editor", "a.txt", hypotheses=["hypothesis-1"])
        memory.store(stored)

        result = memory.retrieve(MemoryQuery(applications=("editor",)))[0]

        self.assertEqual(result.episode["hypotheses"], ["hypothesis-1"])
        self.assertIsNone(result.episode["summary"])
        self.assertNotIn("hypothesis", result.explanation.lower())

    def test_similarity_relations_are_explicit_and_provenanced(self) -> None:
        memory = EpisodicMemory()
        memory.store(episode("episode-1", "editor", "a.txt"))
        memory.store(episode("episode-2", "editor", "b.txt"))

        relations = memory.similarity_relations(minimum_score=0.3)

        self.assertEqual(len(relations), 1)
        self.assertIn("context.application", relations[0]["reason_codes"])
        self.assertEqual(relations[0]["provenance"]["event_ids"], ["episode-1-event", "episode-2-event"])

    def test_persistence_duplicate_and_consolidation_policy_are_explicit(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "episodic-memory.json"
            memory = EpisodicMemory(path, max_episodes=2)
            memory.store(episode("episode-1", "editor", "a.txt"))
            memory.store(episode("episode-2", "browser", "b.html", start_at="2026-01-02T00:00:00Z"))
            self.assertEqual(memory.store(episode("episode-1", "editor", "changed.txt")), StoreResult.duplicate)
            memory.store(episode("episode-3", "terminal", "build.log", start_at="2026-01-03T00:00:00Z"))

            removed = memory.consolidate()
            restored = EpisodicMemory(path, max_episodes=2)

            self.assertEqual(removed, ["episode-1"])
            self.assertEqual(restored.size(), 2)
            persisted = json.loads(path.read_text(encoding="utf-8"))
            self.assertEqual(persisted["schema_version"], "1.0")

    def test_no_context_query_is_deterministic(self) -> None:
        memory = EpisodicMemory()
        memory.store(episode("episode-2", "browser", "b.html", start_at="2026-01-02T00:00:00Z"))
        memory.store(episode("episode-1", "editor", "a.txt"))

        first = [item.episode["episode_id"] for item in memory.retrieve(MemoryQuery())]
        second = [item.episode["episode_id"] for item in memory.retrieve(MemoryQuery())]

        self.assertEqual(first, second)
        self.assertEqual(first, ["episode-1", "episode-2"])


if __name__ == "__main__":
    unittest.main()
