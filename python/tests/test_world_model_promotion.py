import hashlib
import json
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
LAB = ROOT / "python"
if str(LAB) not in sys.path:
    sys.path.insert(0, str(LAB))

from eu_digital_lab.promotion import PromotionManifest


class WorldModelPromotionTests(unittest.TestCase):
    @staticmethod
    def canonical_sha256(path: Path) -> str:
        return hashlib.sha256(path.read_bytes().replace(b"\r\n", b"\n")).hexdigest()

    def test_development_and_holdout_hashes_are_frozen_and_disjoint(self) -> None:
        manifest = PromotionManifest.load(
            ROOT / "promotions" / "cognition.world_model.v1.json"
        )
        development = ROOT / manifest.data["dataset"]["fixture_set"]
        holdout = ROOT / manifest.data["validation"]["holdout_fixture_set"]
        self.assertEqual(
            self.canonical_sha256(development), manifest.data["dataset"]["hash"]
        )
        self.assertEqual(
            self.canonical_sha256(holdout), manifest.data["validation"]["holdout_hash"]
        )
        development_ids = {
            json.loads(line)["case_id"]
            for line in development.read_text().splitlines()
            if line.strip()
        }
        holdout_ids = {
            json.loads(line)["case_id"]
            for line in holdout.read_text().splitlines()
            if line.strip()
        }
        self.assertTrue(development_ids.isdisjoint(holdout_ids))

    def test_native_world_model_remains_unavailable_without_review(self) -> None:
        registry = json.loads(
            (ROOT / "contracts" / "fixtures" / "component_maturity.json").read_text()
        )
        component = next(
            item
            for item in registry["components"]
            if item["component_id"] == "cognition.world_model"
        )
        self.assertEqual(component["reference_status"], "frozen")
        self.assertEqual(component["native_status"], "equivalent")
        self.assertEqual(component["product_status"], "unavailable")
        self.assertEqual(component["promotion_id"], "cognition.world_model.v1")


if __name__ == "__main__":
    unittest.main()
