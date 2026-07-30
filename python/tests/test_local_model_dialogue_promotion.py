import hashlib
import json
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
LAB = ROOT / "python"
if str(LAB) not in sys.path:
    sys.path.insert(0, str(LAB))

from eu_digital_lab.schema_validation import validate_shared_schema


class LocalModelDialoguePromotionTests(unittest.TestCase):
    def test_manifest_freezes_disjoint_fixtures_and_artifact_policy(self) -> None:
        manifest = json.loads(
            (ROOT / "promotions/inference.local_model_gateway.v1.json").read_text(
                encoding="utf-8"
            )
        )
        development = ROOT / manifest["dataset"]["fixture_set"]
        holdout = ROOT / manifest["validation"]["holdout_fixture_set"]
        self.assertEqual(
            hashlib.sha256(development.read_bytes().replace(b"\r\n", b"\n")).hexdigest(),
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
        self.assertEqual(manifest["performance"]["maximum_memory_mb"], 7168.0)

    def test_fixture_requests_and_responses_use_shared_contracts(self) -> None:
        fixture = ROOT / "validation/equivalence/local_model_dialogue_v1.jsonl"
        response_seen = False
        for line in fixture.read_text(encoding="utf-8").splitlines():
            if not line.strip():
                continue
            case = json.loads(line)
            for operation in case.get("operations", []):
                if operation["type"] != "request":
                    continue
                request = operation["request"]
                template = request["template"]
                validate_shared_schema(template, "model_prompt_template.schema.json")
                response_seen = True
        self.assertTrue(response_seen)


if __name__ == "__main__":
    unittest.main()
