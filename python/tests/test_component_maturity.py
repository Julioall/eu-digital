import json
import sys
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
LAB = ROOT / "python"
if str(LAB) not in sys.path:
    sys.path.insert(0, str(LAB))

from eu_digital_lab.component_maturity import (  # noqa: E402
    ComponentMaturityError,
    ComponentMaturityRegistry,
)


class ComponentMaturityTests(unittest.TestCase):
    def test_fixture_is_separate_from_spec_and_promotion_status(self) -> None:
        registry = ComponentMaturityRegistry.load(
            ROOT / "contracts/fixtures/component_maturity.json"
        )
        registry.validate_spec_references(ROOT / "specs")
        registry.validate_evidence_references(ROOT)
        runtime = registry.component("runtime.native_host")
        self.assertEqual(runtime["native_status"], "candidate")
        self.assertEqual(runtime["product_status"], "experimental")
        self.assertIsNone(runtime["promotion_id"])
        cognitive = registry.component("cognition.global_workspace")
        self.assertEqual(cognitive["reference_status"], "python")
        self.assertEqual(cognitive["native_status"], "none")
        self.assertEqual(cognitive["product_status"], "unavailable")

    def test_invalid_promotion_state_is_rejected(self) -> None:
        invalid = json.loads(
            (ROOT / "contracts/fixtures/component_maturity.invalid.json").read_text(
                encoding="utf-8"
            )
        )
        with self.assertRaises(ComponentMaturityError):
            ComponentMaturityRegistry.from_dict(invalid)

    def test_duplicate_component_is_rejected(self) -> None:
        registry = json.loads(
            (ROOT / "contracts/fixtures/component_maturity.json").read_text(
                encoding="utf-8"
            )
        )
        registry["components"].append(dict(registry["components"][0]))
        with self.assertRaises(ComponentMaturityError):
            ComponentMaturityRegistry.from_dict(registry)

    def test_released_component_requires_native_promotion(self) -> None:
        registry = json.loads(
            (ROOT / "contracts/fixtures/component_maturity.json").read_text(
                encoding="utf-8"
            )
        )
        component = registry["components"][0]
        component.update(
            {
                "reference_status": "frozen",
                "native_status": "promoted",
                "product_status": "released",
                "promotion_id": "runtime.promotion.v1",
            }
        )
        valid = ComponentMaturityRegistry.from_dict(registry)
        self.assertEqual(valid.component(component["component_id"])["promotion_id"], "runtime.promotion.v1")

    def test_missing_evidence_is_rejected(self) -> None:
        registry = ComponentMaturityRegistry.load(
            ROOT / "contracts/fixtures/component_maturity.json"
        )
        registry_data = json.loads(json.dumps(registry.data))
        registry_data["components"][0]["evidence_refs"].append("does-not-exist.md")
        loaded = ComponentMaturityRegistry.from_dict(registry_data)
        with self.assertRaises(ComponentMaturityError):
            loaded.validate_evidence_references(ROOT)


if __name__ == "__main__":
    unittest.main()
