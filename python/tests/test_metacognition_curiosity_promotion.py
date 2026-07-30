import hashlib
import json
import sys
import unittest
from pathlib import Path

LAB_ROOT = Path(__file__).resolve().parents[1]
ROOT = LAB_ROOT.parent
if str(LAB_ROOT) not in sys.path:
    sys.path.insert(0, str(LAB_ROOT))

from eu_digital_lab.schema_validation import validate_shared_schema


class MetacognitionCuriosityPromotionTests(unittest.TestCase):
    def test_manifest_freezes_disjoint_fixtures_and_contracts(self) -> None:
        manifest = json.loads(
            (ROOT / "promotions/cognition.metacognition_curiosity.v1.json").read_text(
                encoding="utf-8"
            )
        )
        development = ROOT / manifest["dataset"]["fixture_set"]
        holdout = ROOT / manifest["validation"]["holdout_fixture_set"]
        self.assertEqual(
            hashlib.sha256(
                development.read_bytes().replace(b"\r\n", b"\n")
            ).hexdigest(),
            manifest["dataset"]["hash"],
        )
        self.assertEqual(
            hashlib.sha256(holdout.read_bytes().replace(b"\r\n", b"\n")).hexdigest(),
            manifest["validation"]["holdout_hash"],
        )
        development_cases = [
            json.loads(line)
            for line in development.read_text(encoding="utf-8").splitlines()
            if line.strip()
        ]
        holdout_cases = [
            json.loads(line)
            for line in holdout.read_text(encoding="utf-8").splitlines()
            if line.strip()
        ]
        self.assertFalse(
            {case["case_id"] for case in development_cases}
            & {case["case_id"] for case in holdout_cases}
        )
        self.assertEqual(manifest["dataset"]["case_count"], len(development_cases))
        self.assertEqual(manifest["status"], "reference_frozen")

    def test_fixture_hypotheses_questions_and_responses_validate(self) -> None:
        schemas = {
            "hypothesis": "hypothesis.schema.json",
            "assessment": "metacognitive_assessment.schema.json",
            "question": "curiosity_question.schema.json",
            "response": "curiosity_response.schema.json",
        }
        fixture = ROOT / "validation/equivalence/metacognition_curiosity_v1.jsonl"
        for line in fixture.read_text(encoding="utf-8").splitlines():
            if not line.strip():
                continue
            case = json.loads(line)
            for operation in case["operations"]:
                if operation["type"] == "evaluate":
                    validate_shared_schema(
                        operation["hypothesis"], schemas["hypothesis"]
                    )


if __name__ == "__main__":
    unittest.main()
