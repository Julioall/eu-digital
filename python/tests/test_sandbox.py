import ast
import json
import sys
import unittest
from pathlib import Path

LAB_ROOT = Path(__file__).resolve().parents[1]
if str(LAB_ROOT) not in sys.path:
    sys.path.insert(0, str(LAB_ROOT))

from eu_digital_lab.annotation import (
    AnnotationError,
    annotate_session,
    calculate_agreement,
)
from eu_digital_lab.sandbox import (
    generate_session,
    session_from_dict,
    session_to_dict,
    split_sessions,
)


class SandboxTests(unittest.TestCase):
    def test_same_seed_produces_byte_stable_session(self) -> None:
        first = json.dumps(session_to_dict(generate_session(seed=17, routine_count=3)), sort_keys=True)
        second = json.dumps(session_to_dict(generate_session(seed=17, routine_count=3)), sort_keys=True)
        self.assertEqual(first, second)

    def test_ground_truth_references_are_closed(self) -> None:
        session = generate_session(seed=5, routine_count=4)
        event_ids = {event.event_id for event in session.events}
        episode_ids = {episode.episode_id for episode in session.episodes}
        self.assertTrue(session.events)
        self.assertTrue(session.episodes)
        self.assertTrue(session.causal_links)
        for episode in session.episodes:
            self.assertTrue(set(episode.event_ids) <= event_ids)
            self.assertEqual(episode.session_id, session.session_id)
        for link in session.causal_links:
            self.assertIn(link.event_id, event_ids)
            self.assertIn(link.actor, {"agent", "user", "external", "none"})
        self.assertTrue(set(session_to_dict(session)["episode_ids"]) <= episode_ids)

    def test_round_trip_preserves_ground_truth(self) -> None:
        session = generate_session(seed=9, routine_count=2)
        restored = session_from_dict(session_to_dict(session))
        self.assertEqual(session_to_dict(session), session_to_dict(restored))

    def test_split_is_deterministic_and_disjoint(self) -> None:
        sessions = [generate_session(seed=index, routine_count=2) for index in range(10)]
        first = split_sessions(sessions, seed=23)
        second = split_sessions(sessions, seed=23)
        self.assertEqual(
            {key: [item.session_id for item in value] for key, value in first.items()},
            {key: [item.session_id for item in value] for key, value in second.items()},
        )
        partitions = [item.session_id for values in first.values() for item in values]
        self.assertEqual(len(partitions), len(set(partitions)))
        self.assertEqual(set(partitions), {session.session_id for session in sessions})
        self.assertTrue(first["train"])
        self.assertTrue(first["development"])
        self.assertTrue(first["test"])

    def test_human_annotation_rejects_unknown_episode(self) -> None:
        session = generate_session(seed=3, routine_count=1)
        with self.assertRaises(AnnotationError):
            annotate_session(session, {"episode-does-not-exist": {"label": "routine"}})

    def test_human_annotation_is_structured(self) -> None:
        session = generate_session(seed=3, routine_count=1)
        episode = session.episodes[0]
        result = annotate_session(
            session,
            {
                episode.episode_id: {
                    "label": "routine",
                    "goal": "complete synthetic routine",
                    "relevance": 0.8,
                }
            },
            annotator_id="human-1",
        )
        self.assertEqual(result["schema_version"], "1.0")
        self.assertEqual(result["annotator_id"], "human-1")
        self.assertEqual(result["annotations"][0]["episode_id"], episode.episode_id)

    def test_inter_annotator_agreement_is_calculated(self) -> None:
        session = generate_session(seed=3, routine_count=2)
        labels = {episode.episode_id: {"label": "routine"} for episode in session.episodes}
        first = annotate_session(session, labels, annotator_id="human-1")
        second = annotate_session(session, labels, annotator_id="human-2")
        agreement = calculate_agreement(first, second)
        self.assertEqual(agreement["compared_episodes"], 2)
        self.assertEqual(agreement["exact_agreement"], 1.0)
        self.assertEqual(agreement["cohen_kappa"], 1.0)

    def test_generator_has_no_llm_dependency(self) -> None:
        source = (LAB_ROOT / "eu_digital_lab" / "sandbox.py").read_text(encoding="utf-8")
        tree = ast.parse(source)
        imports = [alias.name for node in ast.walk(tree) if isinstance(node, ast.Import) for alias in node.names]
        imports.extend(node.module or "" for node in ast.walk(tree) if isinstance(node, ast.ImportFrom))
        self.assertFalse(any("llm" in name.lower() for name in imports))


if __name__ == "__main__":
    unittest.main()
