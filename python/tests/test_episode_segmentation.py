import ast
import sys
import unittest
from pathlib import Path

LAB_ROOT = Path(__file__).resolve().parents[1]
if str(LAB_ROOT) not in sys.path:
    sys.path.insert(0, str(LAB_ROOT))

from eu_digital_lab.episode_segmentation import (
    SegmentationError,
    SegmentConfig,
    evaluate_baseline,
    segment_events,
)


def event(
    event_id: str,
    timestamp: str,
    *,
    application: str | None = None,
    document: str | None = None,
    source: str = "system",
    event_type: str = "observation",
) -> dict[str, object]:
    payload: dict[str, object] = {}
    if application is not None:
        payload["application"] = application
    if document is not None:
        payload["document"] = document
    return {
        "event_id": event_id,
        "session_id": "session-1",
        "occurred_at": timestamp,
        "source": source,
        "event_type": event_type,
        "payload": payload,
    }


class EpisodeSegmentationTests(unittest.TestCase):
    def test_every_boundary_has_explainable_reason_and_features_are_retained(self) -> None:
        events = [
            event("e1", "2026-01-01T00:00:00Z", application="editor", document="a.txt"),
            event("e2", "2026-01-01T00:00:01Z", application="editor", document="a.txt", source="input", event_type="key"),
            event("e3", "2026-01-01T00:00:02Z", application="browser", document="b.html", source="ocr", event_type="text"),
            event("e4", "2026-01-01T00:00:10Z", application="browser", document="b.html"),
        ]

        result = segment_events(events, SegmentConfig(max_gap_seconds=3))

        self.assertEqual([episode.event_ids for episode in result.episodes], [["e1", "e2"], ["e3"], ["e4"]])
        self.assertEqual(result.boundaries[0].reasons, ("episode_start",))
        self.assertIn("context_change:application", result.boundaries[1].reasons)
        self.assertIn("context_change:document", result.boundaries[1].reasons)
        self.assertEqual(result.boundaries[2].reasons, ("time_gap",))
        self.assertTrue(all(boundary.reasons for boundary in result.boundaries))
        self.assertEqual(result.episodes[1].context_summary["applications"], ["browser"])
        self.assertEqual(result.episodes[1].context_summary["documents"], ["b.html"])
        self.assertIn("ocr", result.episodes[1].context_summary["modalities"])

    def test_missing_context_is_not_treated_as_a_change(self) -> None:
        events = [
            event("e1", "2026-01-01T00:00:00Z", application="editor"),
            event("e2", "2026-01-01T00:00:01Z"),
            event("e3", "2026-01-01T00:00:02Z", application="editor"),
        ]

        result = segment_events(events, SegmentConfig(max_gap_seconds=3))

        self.assertEqual(len(result.episodes), 1)
        self.assertEqual(result.episodes[0].event_ids, ["e1", "e2", "e3"])

    def test_same_input_and_configuration_are_deterministic(self) -> None:
        events = [
            event("e1", "2026-01-01T00:00:00Z", application="editor"),
            event("e2", "2026-01-01T00:00:04Z", application="editor"),
        ]
        config = SegmentConfig(max_gap_seconds=2)

        first = segment_events(events, config).to_mapping()
        second = segment_events(events, config).to_mapping()

        self.assertEqual(first, second)

    def test_baseline_metric_is_registered_against_annotated_boundaries(self) -> None:
        events = [
            event("e1", "2026-01-01T00:00:00Z", application="editor"),
            event("e2", "2026-01-01T00:00:01Z", application="editor"),
            event("e3", "2026-01-01T00:00:05Z", application="browser"),
            event("e4", "2026-01-01T00:00:06Z", application="browser"),
        ]
        gold = [
            {"event_ids": ["e1", "e2"]},
            {"event_ids": ["e3", "e4"]},
        ]

        metrics = evaluate_baseline(events, gold, SegmentConfig(max_gap_seconds=2))

        self.assertEqual(metrics["baseline_id"], "time_context_threshold_v1")
        self.assertTrue(metrics["registered"])
        self.assertEqual(metrics["boundary_f1"], 1.0)
        self.assertEqual(metrics["window_diff"], 0.0)
        self.assertEqual(metrics["falsification"], "fusion does not exceed the best single-modality baseline")

    def test_context_ablation_is_explicitly_removable(self) -> None:
        events = [
            event("e1", "2026-01-01T00:00:00Z", application="editor"),
            event("e2", "2026-01-01T00:00:01Z", application="browser"),
        ]

        full = segment_events(events, SegmentConfig(max_gap_seconds=10))
        ablated = segment_events(
            events,
            SegmentConfig(
                max_gap_seconds=10,
                split_on_application_change=False,
                split_on_document_change=False,
            ),
        )

        self.assertEqual(len(full.episodes), 2)
        self.assertEqual(len(ablated.episodes), 1)

    def test_invalid_order_and_session_are_rejected(self) -> None:
        with self.assertRaises(SegmentationError):
            segment_events(
                [
                    event("e1", "2026-01-01T00:00:01Z"),
                    event("e2", "2026-01-01T00:00:00Z"),
                ]
            )
        foreign = event("e2", "2026-01-01T00:00:01Z")
        foreign["session_id"] = "session-2"
        with self.assertRaises(SegmentationError):
            segment_events([event("e1", "2026-01-01T00:00:00Z"), foreign])

    def test_segmenter_has_no_llm_dependency(self) -> None:
        source = (LAB_ROOT / "eu_digital_lab" / "episode_segmentation.py").read_text(encoding="utf-8")
        tree = ast.parse(source)
        imports = [alias.name for node in ast.walk(tree) if isinstance(node, ast.Import) for alias in node.names]
        imports.extend(node.module or "" for node in ast.walk(tree) if isinstance(node, ast.ImportFrom))
        self.assertFalse(any("llm" in name.lower() for name in imports))


if __name__ == "__main__":
    unittest.main()
