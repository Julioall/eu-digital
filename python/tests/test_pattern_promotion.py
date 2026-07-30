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


class PatternPromotionTests(unittest.TestCase):
    def test_development_and_holdout_hashes_are_frozen(self) -> None:
        manifest = PromotionManifest.load(REPOSITORY_ROOT / "promotions" / "cognition.pattern_learning.v1.json")
        development = REPOSITORY_ROOT / manifest.data["dataset"]["fixture_set"]
        holdout = REPOSITORY_ROOT / manifest.data["validation"]["holdout_fixture_set"]
        self.assertEqual(hashlib.sha256(development.read_bytes().replace(b"\r\n", b"\n")).hexdigest(), manifest.data["dataset"]["hash"])
        self.assertEqual(hashlib.sha256(holdout.read_bytes().replace(b"\r\n", b"\n")).hexdigest(), manifest.data["validation"]["holdout_hash"])

    def test_equivalent_patterns_are_not_product_available(self) -> None:
        registry = json.loads((REPOSITORY_ROOT / "contracts" / "fixtures" / "component_maturity.json").read_text())
        component = next(item for item in registry["components"] if item["component_id"] == "cognition.pattern_learning")
        self.assertEqual(component["reference_status"], "frozen")
        self.assertEqual(component["native_status"], "equivalent")
        self.assertEqual(component["product_status"], "unavailable")


if __name__ == "__main__":
    unittest.main()
