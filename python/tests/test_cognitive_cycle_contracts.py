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


class CognitiveCycleContractTests(unittest.TestCase):
    def fixture(self, name: str) -> dict[str, object]:
        return json.loads(
            (ROOT / "contracts" / "fixtures" / name).read_text(encoding="utf-8")
        )

    def test_shared_cycle_fixtures_validate(self) -> None:
        validate_shared_schema(
            self.fixture("cognitive_cycle_input.json"),
            "cognitive_cycle_input.schema.json",
        )
        validate_shared_schema(
            self.fixture("cognitive_cycle_result.json"),
            "cognitive_cycle_result.schema.json",
        )

    def test_invalid_state_and_time_basis_are_rejected(self) -> None:
        cycle_input = self.fixture("cognitive_cycle_input.json")
        cycle_input["time_basis"] = "invented"
        with self.assertRaises(SchemaValidationError):
            validate_shared_schema(cycle_input, "cognitive_cycle_input.schema.json")

        result = self.fixture("cognitive_cycle_result.json")
        result["state"] = "processing"
        with self.assertRaises(SchemaValidationError):
            validate_shared_schema(result, "cognitive_cycle_result.schema.json")

    def test_all_spec_045_schemas_are_json_schema_2020_12(self) -> None:
        for name in (
            "cognitive_cycle_input.schema.json",
            "cognitive_cycle_result.schema.json",
            "cognitive_cycle_stage.schema.json",
            "port_invocation_context.schema.json",
            "episode_segmentation_response.schema.json",
            "observation_features.schema.json",
            "salience_assessment.schema.json",
            "hypothesis_formation.schema.json",
        ):
            schema = json.loads(
                (ROOT / "contracts" / "schemas" / name).read_text(encoding="utf-8")
            )
            self.assertEqual(
                schema["$schema"],
                "https://json-schema.org/draft/2020-12/schema",
            )


if __name__ == "__main__":
    unittest.main()
