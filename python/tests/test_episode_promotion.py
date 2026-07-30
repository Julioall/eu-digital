import hashlib
import json
import sys
import unittest
from pathlib import Path

REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
LAB_ROOT = REPOSITORY_ROOT / "python"
if str(LAB_ROOT) not in sys.path:
    sys.path.insert(0, str(LAB_ROOT))

from eu_digital_lab.promotion import PromotionManifest


class EpisodePromotionTests(unittest.TestCase):
    def test_manifest_freezes_development_and_holdout_hashes(self) -> None:
        manifest = PromotionManifest.load(REPOSITORY_ROOT / "promotions" / "cognition.episode_segmentation.v1.json")
        development = REPOSITORY_ROOT / manifest.data["dataset"]["fixture_set"]
        holdout = REPOSITORY_ROOT / manifest.data["validation"]["holdout_fixture_set"]
        self.assertEqual(hashlib.sha256(development.read_bytes().replace(b"\r\n", b"\n")).hexdigest(), manifest.data["dataset"]["hash"])
        self.assertEqual(hashlib.sha256(holdout.read_bytes().replace(b"\r\n", b"\n")).hexdigest(), manifest.data["validation"]["holdout_hash"])
        development_ids = {json.loads(line)["case_id"] for line in development.read_text().splitlines() if line.strip()}
        holdout_ids = {json.loads(line)["case_id"] for line in holdout.read_text().splitlines() if line.strip()}
        self.assertTrue(development_ids)
        self.assertTrue(holdout_ids)
        self.assertTrue(development_ids.isdisjoint(holdout_ids))

    def test_native_equivalence_does_not_make_product_available(self) -> None:
        registry = json.loads((REPOSITORY_ROOT / "contracts" / "fixtures" / "component_maturity.json").read_text())
        component = next(item for item in registry["components"] if item["component_id"] == "cognition.episode_segmentation")
        self.assertEqual(component["reference_status"], "frozen")
        self.assertEqual(component["native_status"], "equivalent")
        self.assertEqual(component["product_status"], "unavailable")


if __name__ == "__main__":
    unittest.main()
