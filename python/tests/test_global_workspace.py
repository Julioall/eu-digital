import ast
import asyncio
import sys
import unittest
from pathlib import Path

LAB_ROOT = Path(__file__).resolve().parents[1]
if str(LAB_ROOT) not in sys.path:
    sys.path.insert(0, str(LAB_ROOT))

from eu_digital_lab.event_bus import AsyncEventBus
from eu_digital_lab.global_workspace import (
    ABLATION,
    BASELINE_ID,
    FALSIFICATION,
    GlobalWorkspace,
    WorkspaceCandidate,
    WorkspaceConfig,
    WorkspaceError,
    evaluate_selection,
)
from eu_digital_lab.schema_validation import (
    SchemaValidationError,
    validate_shared_schema,
)

TIME_ZERO = "2026-07-28T12:00:00+00:00"


def candidate(
    candidate_id: str,
    signals: dict[str, float],
    *,
    source_kind: str = "episode",
) -> WorkspaceCandidate:
    return WorkspaceCandidate(
        candidate_id=candidate_id,
        session_id="session-1",
        source_kind=source_kind,
        source_refs=(f"{source_kind}-{candidate_id}",),
        observed_at=TIME_ZERO,
        content={"label": candidate_id},
        salience_signals=signals,
    )


class GlobalWorkspaceTests(unittest.TestCase):
    def workspace(self, config: WorkspaceConfig | None = None) -> GlobalWorkspace:
        return GlobalWorkspace("workspace-1", "session-1", config=config)

    def test_candidates_compete_for_bounded_capacity_with_auditable_decisions(self) -> None:
        workspace = self.workspace(WorkspaceConfig(capacity=2, ttl_seconds=60))
        workspace.admit(candidate("low", {"novelty": 0.1}), TIME_ZERO)
        workspace.admit(candidate("middle", {"priority": 0.5}), TIME_ZERO)
        snapshot = workspace.admit(candidate("high", {"risk": 0.9}), TIME_ZERO)

        self.assertEqual(
            [item.candidate_id for item in snapshot.active_items],
            ["high", "middle"],
        )
        decisions = {decision.candidate_id: decision for decision in snapshot.decisions}
        self.assertTrue(decisions["high"].selected)
        self.assertFalse(decisions["low"].selected)
        self.assertIn("selection.capacity", decisions["high"].reason_codes)
        self.assertIn("capacity.excluded", decisions["low"].reason_codes)
        self.assertEqual(snapshot.to_mapping()["capacity"], 2)

    def test_explicit_priority_change_recomputes_active_contents(self) -> None:
        workspace = self.workspace(WorkspaceConfig(capacity=1, ttl_seconds=60))
        workspace.admit(candidate("first", {"priority": 0.6}), TIME_ZERO)
        workspace.admit(candidate("second", {"priority": 0.5}), TIME_ZERO)

        updated = workspace.update_priority("second", 0.9, TIME_ZERO)

        self.assertEqual([item.candidate_id for item in updated.active_items], ["second"])
        self.assertEqual(updated.active_items[0].salience.observed_factors["priority"], 0.9)
        self.assertEqual(updated.selection_churn, 1.0)

    def test_equal_scores_use_candidate_id_as_a_deterministic_tie_breaker(self) -> None:
        workspace = self.workspace(WorkspaceConfig(capacity=1, ttl_seconds=60))
        workspace.admit(candidate("later-id", {"novelty": 0.5}), TIME_ZERO)

        snapshot = workspace.admit(candidate("earlier-id", {"novelty": 0.5}), TIME_ZERO)

        self.assertEqual([item.candidate_id for item in snapshot.active_items], ["earlier-id"])

    def test_expiration_removes_old_items_and_reports_it(self) -> None:
        workspace = self.workspace(WorkspaceConfig(capacity=2, ttl_seconds=5))
        workspace.admit(candidate("short-lived", {"novelty": 0.8}), TIME_ZERO)

        expired = workspace.snapshot("2026-07-28T12:00:06+00:00")

        self.assertEqual(expired.active_items, ())
        self.assertEqual(expired.expired_candidate_ids, ("short-lived",))

    def test_missing_signal_is_explicit_and_never_scored_as_negative(self) -> None:
        workspace = self.workspace(WorkspaceConfig(capacity=1, ttl_seconds=60))

        snapshot = workspace.admit(candidate("observed", {"novelty": 0.6}), TIME_ZERO)

        item = snapshot.active_items[0]
        self.assertEqual(item.salience.score, 0.6)
        self.assertEqual(item.salience.observed_factors, {"novelty": 0.6})
        self.assertIn("priority", item.salience.missing_factors)

    def test_workspace_item_contract_rejects_score_above_one(self) -> None:
        workspace = self.workspace(WorkspaceConfig(capacity=1, ttl_seconds=60))
        item = workspace.admit(candidate("bounded", {"novelty": 0.6}), TIME_ZERO).active_items[0]
        invalid = item.to_mapping()
        invalid["salience"]["score"] = 1.1

        with self.assertRaises(SchemaValidationError):
            validate_shared_schema(invalid, "workspace_item.schema.json")

    def test_broadcast_is_a_local_canonical_event_with_validated_snapshot(self) -> None:
        async def scenario() -> tuple[dict[str, object], dict[str, object]]:
            workspace = self.workspace(WorkspaceConfig(capacity=1, ttl_seconds=60))
            snapshot = workspace.admit(candidate("broadcast", {"direct_mention": 1.0}), TIME_ZERO)
            bus = AsyncEventBus()
            subscription = bus.subscribe(event_type="workspace.selection.v1")
            event = await workspace.broadcast(snapshot, bus.publish, TIME_ZERO)
            received = await subscription.get()
            return event, received

        event, received = asyncio.run(scenario())

        self.assertEqual(event["event_type"], "workspace.selection.v1")
        self.assertEqual(received["payload"], event["payload"])
        self.assertEqual(received["payload"]["snapshot"]["active_items"][0]["candidate_id"], "broadcast")

    def test_order_and_positive_weight_scale_are_metamorphic_invariants(self) -> None:
        left = self.workspace(WorkspaceConfig(capacity=2, ttl_seconds=60))
        right = self.workspace(WorkspaceConfig(capacity=2, ttl_seconds=60))
        candidates = [
            candidate("a", {"novelty": 0.8, "priority": 0.2}),
            candidate("b", {"novelty": 0.4, "priority": 0.9}),
            candidate("c", {"risk": 0.6}),
        ]
        for value in candidates:
            left.admit(value, TIME_ZERO)
        for value in reversed(candidates):
            right.admit(value, TIME_ZERO)

        self.assertEqual(left.snapshot(TIME_ZERO).to_mapping(), right.snapshot(TIME_ZERO).to_mapping())

        original = WorkspaceConfig(capacity=2, ttl_seconds=60)
        scaled = WorkspaceConfig(
            capacity=2,
            ttl_seconds=60,
            weights={name: weight * 7 for name, weight in original.weights.items()},
        )
        baseline_workspace = self.workspace(original)
        scaled_workspace = self.workspace(scaled)
        for value in candidates:
            baseline_workspace.admit(value, TIME_ZERO)
            scaled_workspace.admit(value, TIME_ZERO)
        self.assertEqual(
            [item.candidate_id for item in baseline_workspace.snapshot(TIME_ZERO).active_items],
            [item.candidate_id for item in scaled_workspace.snapshot(TIME_ZERO).active_items],
        )

    def test_resource_bound_ablation_and_scientific_metadata_are_explicit(self) -> None:
        workspace = self.workspace(WorkspaceConfig(capacity=1, max_candidates=2, ttl_seconds=60))
        workspace.admit(candidate("discard", {"novelty": 0.1}), TIME_ZERO)
        workspace.admit(candidate("keep", {"novelty": 0.8}), TIME_ZERO)
        snapshot = workspace.admit(candidate("top", {"novelty": 0.9}), TIME_ZERO)
        metrics = evaluate_selection(snapshot, {"top"})
        ablated = self.workspace(
            WorkspaceConfig(capacity=1, ttl_seconds=60).without_factors("priority")
        )

        self.assertEqual(snapshot.discarded_candidate_ids, ("discard",))
        self.assertEqual(metrics["baseline_id"], BASELINE_ID)
        self.assertEqual(metrics["ablation"], ABLATION)
        self.assertEqual(metrics["falsification"], FALSIFICATION)
        self.assertEqual(metrics["precision_at_capacity"], 1.0)
        self.assertNotIn("priority", ablated.config.enabled_factors)

    def test_fifo_baseline_uses_the_same_workspace_interface(self) -> None:
        workspace = self.workspace(
            WorkspaceConfig(capacity=1, ttl_seconds=60, selection_policy=BASELINE_ID)
        )
        workspace.admit(candidate("first", {"priority": 0.1}), TIME_ZERO)
        snapshot = workspace.admit(candidate("later", {"priority": 1.0}), TIME_ZERO)

        self.assertEqual([item.candidate_id for item in snapshot.active_items], ["first"])
        self.assertEqual(snapshot.policy_id, BASELINE_ID)
        self.assertIn("selection.fifo_admission", snapshot.active_items[0].selection_reasons)

    def test_invalid_candidates_and_sessions_are_typed_errors(self) -> None:
        with self.assertRaises(WorkspaceError):
            candidate("invalid", {})
        with self.assertRaises(WorkspaceError):
            self.workspace().admit(
                WorkspaceCandidate(
                    candidate_id="wrong-session",
                    session_id="session-2",
                    source_kind="episode",
                    source_refs=("episode-1",),
                    observed_at=TIME_ZERO,
                    content={},
                    salience_signals={"novelty": 0.2},
                ),
                TIME_ZERO,
            )

    def test_workspace_has_no_llm_dependency(self) -> None:
        source = (LAB_ROOT / "eu_digital_lab" / "global_workspace.py").read_text(encoding="utf-8")
        tree = ast.parse(source)
        imports = [alias.name for node in ast.walk(tree) if isinstance(node, ast.Import) for alias in node.names]
        imports.extend(node.module or "" for node in ast.walk(tree) if isinstance(node, ast.ImportFrom))
        self.assertFalse(any("llm" in name.lower() for name in imports))


if __name__ == "__main__":
    unittest.main()
