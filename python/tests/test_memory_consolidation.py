import ast
import sys
import unittest
from pathlib import Path

LAB_ROOT = Path(__file__).resolve().parents[1]
if str(LAB_ROOT) not in sys.path:
    sys.path.insert(0, str(LAB_ROOT))

from eu_digital_lab.memory_consolidation import (
    ABLATION,
    BASELINE_POLICY_ID,
    CONSOLIDATION_POLICY_ID,
    FALSIFICATION,
    HYPOTHESIS,
    ConsolidationPolicy,
    MemoryConsolidationError,
    MemoryConsolidator,
)

TIME_ZERO = "2026-01-01T00:00:00+00:00"


def episode(
    episode_id: str,
    application: str,
    *,
    start_at: str = TIME_ZERO,
    hypotheses: list[str] | None = None,
) -> dict[str, object]:
    return {
        "episode_id": episode_id,
        "schema_version": "1.0",
        "session_id": "session-1",
        "start_at": start_at,
        "end_at": start_at,
        "event_ids": [f"{episode_id}-event"],
        "context_summary": {
            "applications": [application],
            "documents": [],
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


class MemoryConsolidationTests(unittest.TestCase):
    def test_replay_creates_schema_backed_knowledge_with_provenance(self) -> None:
        engine = MemoryConsolidator()
        record = engine.replay(
            [episode("episode-1", "editor"), episode("episode-2", "editor")],
            "2026-01-02T00:00:00+00:00",
        )
        self.assertEqual(record.policy_id, CONSOLIDATION_POLICY_ID)
        self.assertEqual(record.source_episode_ids, ("episode-1", "episode-2"))
        knowledge = engine.knowledge("applications:editor")[0]
        self.assertEqual(knowledge.support_count, 2)
        self.assertEqual(knowledge.source_episode_ids, ("episode-1", "episode-2"))
        self.assertGreaterEqual(knowledge.version, 1)

    def test_reconciliation_preserves_alternatives_and_contradictions(self) -> None:
        engine = MemoryConsolidator()
        engine.replay(
            [
                episode("episode-1", "editor", hypotheses=["routine-a"]),
                episode(
                    "episode-2",
                    "editor",
                    start_at="2026-01-02T00:00:00+00:00",
                    hypotheses=["routine-b"],
                ),
            ],
            "2026-01-03T00:00:00+00:00",
        )
        knowledge = engine.knowledge("applications:editor")[0]
        self.assertEqual(knowledge.alternatives, ("hypothesis:routine-a", "hypothesis:routine-b"))
        self.assertEqual(knowledge.contradictions, ("multiple_hypotheses",))

    def test_baseline_ablation_does_not_create_knowledge(self) -> None:
        engine = MemoryConsolidator(ConsolidationPolicy.no_replay_v0)
        record = engine.replay([episode("episode-1", "editor")], "2026-01-02T00:00:00+00:00")
        self.assertEqual(record.policy_id, BASELINE_POLICY_ID)
        self.assertEqual(record.knowledge_ids, ())
        self.assertEqual(engine.knowledge(), ())

    def test_replay_improves_retention_against_baseline_interface(self) -> None:
        treatment = MemoryConsolidator()
        baseline = MemoryConsolidator(ConsolidationPolicy.no_replay_v0)
        source = [episode("episode-1", "editor")]
        treatment.replay(source, "2026-01-02T00:00:00+00:00")
        baseline.replay(source, "2026-01-02T00:00:00+00:00")
        expected = ("applications:editor",)
        self.assertGreater(
            float(treatment.metrics(expected)["retention_score"]),
            float(baseline.metrics(expected)["retention_score"]),
        )

    def test_retention_archiving_is_reversible_and_does_not_delete_source(self) -> None:
        engine = MemoryConsolidator(max_active_episodes=1)
        source = [
            episode("episode-1", "editor"),
            episode("episode-2", "browser", start_at="2026-01-02T00:00:00+00:00"),
        ]
        engine.replay(source, "2026-01-03T00:00:00+00:00")
        decisions = engine.apply_retention("2026-01-03T00:00:01+00:00")
        self.assertEqual(engine.archived_episode_ids, ("episode-1",))
        self.assertEqual([item.action for item in decisions], ["archive", "retain"])
        restored = engine.restore("episode-1", "2026-01-03T00:00:02+00:00")
        self.assertEqual(restored.action, "restore")
        self.assertEqual(engine.archived_episode_ids, ())
        self.assertEqual(engine.knowledge("applications:editor")[0].source_episode_ids, ("episode-1",))

    def test_replay_increments_version_without_losing_sources(self) -> None:
        engine = MemoryConsolidator()
        engine.replay([episode("episode-1", "editor")], "2026-01-02T00:00:00+00:00")
        engine.replay(
            [episode("episode-2", "editor", start_at="2026-01-02T00:00:00+00:00")],
            "2026-01-03T00:00:00+00:00",
        )
        knowledge = engine.knowledge("applications:editor")[0]
        self.assertEqual(knowledge.version, 2)
        self.assertEqual(knowledge.source_episode_ids, ("episode-1", "episode-2"))

    def test_metadata_and_invalid_restore_are_explicit(self) -> None:
        with self.assertRaises(MemoryConsolidationError):
            MemoryConsolidator().restore("missing", "2026-01-01T00:00:00+00:00")
        metadata = MemoryConsolidator.scientific_metadata()
        self.assertEqual(metadata["hypothesis"], HYPOTHESIS)
        self.assertEqual(metadata["ablation"], ABLATION)
        self.assertEqual(metadata["falsification"], FALSIFICATION)

    def test_module_has_no_llm_or_external_service_import(self) -> None:
        source = (LAB_ROOT / "eu_digital_lab" / "memory_consolidation.py").read_text(
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
        self.assertFalse(
            any(term in name.lower() for name in imports for term in ("llm", "requests", "http"))
        )


if __name__ == "__main__":
    unittest.main()
