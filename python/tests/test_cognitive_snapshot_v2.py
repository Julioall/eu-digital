import json
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
LAB_ROOT = ROOT / "python"
if str(LAB_ROOT) not in sys.path:
    sys.path.insert(0, str(LAB_ROOT))

from eu_digital_lab.schema_validation import (
    SchemaValidationError,
    validate_shared_schema,
)


class CognitiveSnapshotV2ContractTests(unittest.TestCase):
    def fixture(self) -> dict[str, object]:
        return json.loads(
            (
                ROOT
                / "contracts"
                / "fixtures"
                / "cognitive_snapshot_v2.json"
            ).read_text(encoding="utf-8")
        )

    def test_fixture_validates_with_external_state_schema(self) -> None:
        validate_shared_schema(
            self.fixture(), "cognitive_snapshot_v2.schema.json"
        )

    def test_duplicate_provider_and_empty_entries_are_rejected(self) -> None:
        duplicate = self.fixture()
        state = duplicate["state"]
        assert isinstance(state, dict)
        state["required_provider_ids"] = [
            "episode_boundary_impl",
            "episode_boundary_impl",
        ]
        with self.assertRaises(SchemaValidationError):
            validate_shared_schema(duplicate, "cognitive_snapshot_v2.schema.json")

        empty = self.fixture()
        empty_state = empty["state"]
        assert isinstance(empty_state, dict)
        fragments = empty_state["fragments"]
        assert isinstance(fragments, list)
        fragment = fragments[0]
        assert isinstance(fragment, dict)
        fragment["entries"] = {}
        with self.assertRaises(SchemaValidationError):
            validate_shared_schema(empty, "cognitive_snapshot_v2.schema.json")

        invalid_time = self.fixture()
        invalid_time["captured_at"] = "not-a-date"
        with self.assertRaises(SchemaValidationError):
            validate_shared_schema(
                invalid_time, "cognitive_snapshot_v2.schema.json"
            )


if __name__ == "__main__":
    unittest.main()
