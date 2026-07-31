import json
import pytest

from eu_digital_lab.snapshot import CognitiveSnapshot, SnapshotError


def test_cognitive_snapshot_generation():
    payload = {"state": "active", "value": 42}
    snapshot = CognitiveSnapshot.create(
        configuration_fingerprint="test-fingerprint",
        last_applied_event_id="evt-001",
        payload=payload,
    )
    
    assert snapshot.configuration_fingerprint == "test-fingerprint"
    assert snapshot.last_applied_event_id == "evt-001"
    assert snapshot.payload == payload
    
    # Verify the checksum is populated
    assert len(snapshot.data["checksum"]) == 64
    
    # Should be valid to recreate from mapping
    recreated = CognitiveSnapshot.from_mapping(snapshot.data)
    assert recreated.data == snapshot.data


def test_cognitive_snapshot_corruption():
    payload = {"state": "active"}
    snapshot = CognitiveSnapshot.create(
        configuration_fingerprint="test-fingerprint",
        last_applied_event_id="evt-001",
        payload=payload,
    )
    
    corrupted_data = dict(snapshot.data)
    corrupted_data["payload"]["state"] = "inactive"
    
    with pytest.raises(SnapshotError, match="snapshot checksum mismatch"):
        CognitiveSnapshot.from_mapping(corrupted_data)


def test_cognitive_snapshot_missing_fields():
    with pytest.raises(SnapshotError):
        CognitiveSnapshot.from_mapping({"invalid": "data"})
