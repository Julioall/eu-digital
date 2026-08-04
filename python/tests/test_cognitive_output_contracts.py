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


class CognitiveOutputContractTests(unittest.TestCase):
    def fixture(self, name: str) -> dict[str, object]:
        return json.loads(
            (ROOT / "contracts" / "fixtures" / name).read_text(encoding="utf-8")
        )

    def test_shared_fixtures_validate(self) -> None:
        for fixture, schema in (
            ("cognitive_output_request.json", "cognitive_output_request.schema.json"),
            (
                "language_rendering_candidate.json",
                "language_rendering_candidate.schema.json",
            ),
            ("cognitive_output.json", "cognitive_output.schema.json"),
        ):
            validate_shared_schema(self.fixture(fixture), schema)

    def test_unknown_fields_and_invalid_state_are_rejected(self) -> None:
        candidate = self.fixture("language_rendering_candidate.json")
        candidate["unexpected"] = True
        with self.assertRaises(SchemaValidationError):
            validate_shared_schema(
                candidate, "language_rendering_candidate.schema.json"
            )

        output = self.fixture("cognitive_output.json")
        output["status"] = "invented"
        with self.assertRaises(SchemaValidationError):
            validate_shared_schema(output, "cognitive_output.schema.json")

        request = self.fixture("cognitive_output_request.json")
        request["evidence_refs"].append(request["evidence_refs"][0])
        with self.assertRaises(SchemaValidationError):
            validate_shared_schema(request, "cognitive_output_request.schema.json")

    def test_all_schemas_are_2020_12_and_strict(self) -> None:
        for name in (
            "cognitive_output_request.schema.json",
            "language_rendering_candidate.schema.json",
            "cognitive_output.schema.json",
        ):
            schema = json.loads(
                (ROOT / "contracts" / "schemas" / name).read_text(encoding="utf-8")
            )
            self.assertEqual(
                schema["$schema"],
                "https://json-schema.org/draft/2020-12/schema",
            )
            self.assertFalse(schema["additionalProperties"])


if __name__ == "__main__":
    unittest.main()
