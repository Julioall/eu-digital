import unittest

from eu_digital_lab.snapshot import CognitiveSnapshot, SnapshotError


class TestCognitiveSnapshot(unittest.TestCase):
    def test_cognitive_snapshot_generation(self):
        payload = {"state": "active", "value": 42}
        snapshot = CognitiveSnapshot.create(
            configuration_fingerprint="test-fingerprint",
            last_applied_event_id="evt-001",
            payload=payload,
        )
        
        self.assertEqual(snapshot.configuration_fingerprint, "test-fingerprint")
        self.assertEqual(snapshot.last_applied_event_id, "evt-001")
        self.assertEqual(snapshot.payload, payload)
        
        # Verify the checksum is populated
        self.assertEqual(len(snapshot.data["checksum"]), 64)
        
        # Should be valid to recreate from mapping
        recreated = CognitiveSnapshot.from_mapping(snapshot.data)
        self.assertEqual(recreated.data, snapshot.data)

    def test_cognitive_snapshot_corruption(self):
        payload = {"state": "active"}
        snapshot = CognitiveSnapshot.create(
            configuration_fingerprint="test-fingerprint",
            last_applied_event_id="evt-001",
            payload=payload,
        )
        
        corrupted_data = dict(snapshot.data)
        corrupted_data["payload"]["state"] = "inactive"
        
        with self.assertRaisesRegex(SnapshotError, "snapshot checksum mismatch"):
            CognitiveSnapshot.from_mapping(corrupted_data)

    def test_cognitive_snapshot_missing_fields(self):
        with self.assertRaises(SnapshotError):
            CognitiveSnapshot.from_mapping({"invalid": "data"})

