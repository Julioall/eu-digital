import json
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
LAB = ROOT / "python"
if str(LAB) not in sys.path:
    sys.path.insert(0, str(LAB))

from eu_digital_lab.observation_policy import (
    ObservationPolicyError,
    load_capture_policy,
    validate_capture_policy,
)


class ObservationPolicyTests(unittest.TestCase):
    def test_default_policy_disables_textual_capture(self) -> None:
        policy = load_capture_policy(ROOT / "contracts/fixtures/capture_policy.json")
        self.assertFalse(policy["capture_window_title"])
        self.assertFalse(policy["capture_clipboard"])
        self.assertEqual(policy["allowlist"], [])

    def test_textual_capture_requires_allowlist(self) -> None:
        policy = load_capture_policy(ROOT / "contracts/fixtures/capture_policy.json")
        value = json.loads(json.dumps(policy))
        value["capture_window_title"] = True
        with self.assertRaises(ObservationPolicyError):
            validate_capture_policy(value)

    def test_incomplete_mandatory_denylist_is_rejected(self) -> None:
        with self.assertRaises(ObservationPolicyError):
            load_capture_policy(ROOT / "contracts/fixtures/capture_policy.invalid.json")

    def test_valid_explicit_textual_policy(self) -> None:
        policy = load_capture_policy(ROOT / "contracts/fixtures/capture_policy.json")
        policy["allowlist"].append("editor.exe")
        policy["capture_window_title"] = True
        policy["capture_clipboard"] = True
        validate_capture_policy(policy)


if __name__ == "__main__":
    unittest.main()
