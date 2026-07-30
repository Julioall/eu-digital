import hashlib
import json
import sys
import unittest
from pathlib import Path

LAB_ROOT = Path(__file__).resolve().parents[1]
ROOT = LAB_ROOT.parent
if str(LAB_ROOT) not in sys.path:
    sys.path.insert(0, str(LAB_ROOT))
if str(ROOT / "tools") not in sys.path:
    sys.path.insert(0, str(ROOT / "tools"))

from validate_functional_self_model_promotion import (
    invariant_failures,
    reference_transform,
)


class FunctionalSelfModelPromotionTests(unittest.TestCase):
    def test_frozen_fixture_hashes_and_holdout_are_disjoint(self) -> None:
        manifest = json.loads(
            (ROOT / "promotions/cognition.functional_self_model.v1.json").read_text(
                encoding="utf-8"
            )
        )
        development = ROOT / manifest["dataset"]["fixture_set"]
        holdout = ROOT / manifest["validation"]["holdout_fixture_set"]

        def digest(path: Path) -> str:
            return hashlib.sha256(path.read_bytes().replace(b"\r\n", b"\n")).hexdigest()

        self.assertEqual(digest(development), manifest["dataset"]["hash"])
        self.assertEqual(digest(holdout), manifest["validation"]["holdout_hash"])
        development_ids = {
            json.loads(line)["case_id"]
            for line in development.read_text(encoding="utf-8").splitlines()
            if line.strip()
        }
        holdout_ids = {
            json.loads(line)["case_id"]
            for line in holdout.read_text(encoding="utf-8").splitlines()
            if line.strip()
        }
        self.assertFalse(development_ids & holdout_ids)

    def test_reference_fixtures_preserve_history_and_gate_invariants(self) -> None:
        fixture = ROOT / "validation/equivalence/functional_self_model_v1.jsonl"
        for line in fixture.read_text(encoding="utf-8").splitlines():
            if not line.strip():
                continue
            case = json.loads(line)
            result = reference_transform(case)
            self.assertEqual(invariant_failures(case, result), [])
            self.assertEqual(result["schema_version"], "1.0")


if __name__ == "__main__":
    unittest.main()
