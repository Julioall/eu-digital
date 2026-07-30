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


class EpisodicMemoryPromotionTests(unittest.TestCase):
    @staticmethod
    def canonical_sha256(path: Path) -> str:
        return hashlib.sha256(path.read_bytes().replace(b"\r\n", b"\n")).hexdigest()

    def test_development_and_holdout_hashes_are_frozen_and_disjoint(self) -> None:
        manifest = PromotionManifest.load(REPOSITORY_ROOT / "promotions" / "cognition.episodic_memory.v1.json")
        development = REPOSITORY_ROOT / manifest.data["dataset"]["fixture_set"]
        holdout = REPOSITORY_ROOT / manifest.data["validation"]["holdout_fixture_set"]
        reference = REPOSITORY_ROOT / manifest.data["reference"]["source_path"]
        self.assertEqual(self.canonical_sha256(reference), manifest.data["reference"]["source_sha256"])
        self.assertEqual(self.canonical_sha256(development), manifest.data["dataset"]["hash"])
        self.assertEqual(self.canonical_sha256(holdout), manifest.data["validation"]["holdout_hash"])
        development_ids = {json.loads(line)["case_id"] for line in development.read_text().splitlines() if line.strip()}
        holdout_ids = {json.loads(line)["case_id"] for line in holdout.read_text().splitlines() if line.strip()}
        self.assertTrue(development_ids.isdisjoint(holdout_ids))

    def test_report_keeps_ground_truth_schema_and_metamorphic_boundaries(self) -> None:
        report = json.loads((REPOSITORY_ROOT / "validation" / "reports" / "episodic_memory_v1.json").read_text())
        self.assertEqual(report["reference"]["source_sha256"], report["manifest"]["reference"]["source_sha256"])
        self.assertEqual(report["ground_truth"]["development"]["recall_at_k"], 1.0)
        self.assertTrue(report["metamorphic"]["passed"])
        self.assertEqual(report["invariants"]["failures"], {})

    def test_equivalent_native_memory_is_not_product_available(self) -> None:
        registry = json.loads((REPOSITORY_ROOT / "contracts" / "fixtures" / "component_maturity.json").read_text())
        component = next(item for item in registry["components"] if item["component_id"] == "cognition.episodic_memory")
        self.assertEqual(component["reference_status"], "frozen")
        self.assertEqual(component["native_status"], "equivalent")
        self.assertEqual(component["product_status"], "unavailable")


if __name__ == "__main__":
    unittest.main()
