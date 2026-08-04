import json
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
LAB = ROOT / "python"
if str(LAB) not in sys.path:
    sys.path.insert(0, str(LAB))

from eu_digital_lab.privacy_storage import (
    ConsentPolicy,
    PrivacyStorageError,
    StoragePolicy,
    validate_data_management_request,
    validate_privacy_storage_fixtures,
    validate_storage_health,
)


class PrivacyStorageTests(unittest.TestCase):
    def test_valid_fixtures_and_default_denial(self) -> None:
        validate_privacy_storage_fixtures(ROOT)
        consent = ConsentPolicy.load(ROOT / "contracts/fixtures/consent_policy.json")
        self.assertTrue(consent.allows("system_activity", "local_activity_observation"))
        self.assertFalse(consent.allows("screen_ocr", "screen_text_observation"))
        self.assertFalse(consent.allows("audio", "anything_not_granted"))

    def test_global_pause_overrides_grant(self) -> None:
        consent = ConsentPolicy.load(ROOT / "contracts/fixtures/consent_policy.json")
        value = json.loads(json.dumps(consent.data))
        value["global_pause"] = True
        self.assertFalse(ConsentPolicy.from_mapping(value).allows("system_activity", "local_activity_observation"))

    def test_invalid_consent_fixture_is_rejected(self) -> None:
        with self.assertRaises(PrivacyStorageError):
            ConsentPolicy.load(ROOT / "contracts/fixtures/consent_policy.invalid.json")

    def test_storage_policy_requires_all_buckets_and_separate_model(self) -> None:
        policy = StoragePolicy.load(ROOT / "contracts/fixtures/storage_policy.json")
        self.assertEqual(policy.data["raw_event_retention_days"], 30)
        self.assertEqual(policy.data["derived_memory_retention_days"], 365)
        self.assertEqual(policy.data["quarantine_retention_days"], 14)
        self.assertEqual(policy.data["user_storage_quota_gib"], 10)
        value = json.loads(json.dumps(policy.data))
        value["included_buckets"] = ["database"]
        with self.assertRaises(PrivacyStorageError):
            StoragePolicy.from_mapping(value)

    def test_invalid_storage_fixture_is_rejected(self) -> None:
        with self.assertRaises(PrivacyStorageError):
            StoragePolicy.load(ROOT / "contracts/fixtures/storage_policy.invalid.json")

    def test_quota_health_cannot_hide_capture_block(self) -> None:
        value = json.loads(
            (ROOT / "contracts/fixtures/storage_health.json").read_text(encoding="utf-8")
        )
        value["capture_suspended"] = False
        with self.assertRaises(PrivacyStorageError):
            validate_storage_health(value)

    def test_unconfirmed_management_request_is_rejected(self) -> None:
        value = json.loads(
            (ROOT / "contracts/fixtures/data_management_request.invalid.json").read_text(
                encoding="utf-8"
            )
        )
        with self.assertRaises(PrivacyStorageError):
            validate_data_management_request(value)


if __name__ == "__main__":
    unittest.main()
