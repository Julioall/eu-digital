import json
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
LAB = ROOT / "python"
if str(LAB) not in sys.path:
    sys.path.insert(0, str(LAB))

from eu_digital_lab.screen_ocr_policy import (
    ScreenOcrPolicy,
    ScreenOcrPolicyError,
    validate_screen_ocr_fixtures,
)


class ScreenOcrPolicyTests(unittest.TestCase):
    def test_fixtures_and_default_deny_are_validated(self) -> None:
        validate_screen_ocr_fixtures(ROOT)
        default = ScreenOcrPolicy.load(
            ROOT / "contracts/fixtures/screen_ocr_capture_policy.default.json"
        )
        self.assertFalse(default.allows_capture())
        self.assertFalse(default.allows_capture("request-1"))

    def test_explicit_and_on_demand_authorization_are_distinct(self) -> None:
        explicit = ScreenOcrPolicy.load(ROOT / "contracts/fixtures/screen_ocr_capture_policy.json")
        self.assertTrue(explicit.allows_capture())
        on_demand = json.loads(json.dumps(explicit.data))
        on_demand["capture_mode"] = "on_demand"
        on_demand["request_id"] = "request-1"
        policy = ScreenOcrPolicy.from_mapping(on_demand)
        self.assertFalse(policy.allows_capture())
        self.assertTrue(policy.allows_capture("request-1"))

    def test_revocation_and_pause_block_capture(self) -> None:
        value = json.loads(
            (ROOT / "contracts/fixtures/screen_ocr_capture_policy.json").read_text(encoding="utf-8")
        )
        value["consent_state"] = "revoked"
        value["capture_mode"] = "disabled"
        revoked = ScreenOcrPolicy.from_mapping(value)
        self.assertFalse(revoked.allows_capture())
        value["consent_state"] = "granted"
        value["capture_mode"] = "explicit"
        value["global_pause"] = True
        paused = ScreenOcrPolicy.from_mapping(value)
        self.assertFalse(paused.allows_capture())

    def test_text_is_redacted_and_shorter_retention_is_required(self) -> None:
        policy = ScreenOcrPolicy.load(ROOT / "contracts/fixtures/screen_ocr_capture_policy.json")
        redacted = policy.redact_text("senha-secreta")
        self.assertNotIn("senha-secreta", redacted)
        self.assertEqual(redacted, "[redacted:length=13]")
        invalid = dict(policy.data)
        invalid["text_retention_days"] = invalid["visual_retention_days"]
        with self.assertRaises(ScreenOcrPolicyError):
            ScreenOcrPolicy.from_mapping(invalid)


if __name__ == "__main__":
    unittest.main()
