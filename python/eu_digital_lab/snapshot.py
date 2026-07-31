"""Reference validation for Cognitive Snapshot."""

from __future__ import annotations

import copy
import hashlib
import json
from dataclasses import dataclass
from datetime import datetime, timezone
from typing import Any

from .schema_validation import SchemaValidationError, validate_shared_schema


class SnapshotError(ValueError):
    """Raised when a cognitive snapshot is invalid or corrupted."""


@dataclass(frozen=True)
class CognitiveSnapshot:
    data: dict[str, Any]

    @classmethod
    def create(
        cls,
        configuration_fingerprint: str,
        last_applied_event_id: str,
        payload: dict[str, Any],
    ) -> CognitiveSnapshot:
        # Create without checksum first
        raw = {
            "schema_version": "1.0",
            "timestamp": datetime.now(timezone.utc).isoformat(timespec="seconds"),
            "checksum": "",
            "configuration_fingerprint": configuration_fingerprint,
            "last_applied_event_id": last_applied_event_id,
            "payload": payload,
        }
        
        # Calculate checksum over the deterministic JSON string, omitting the checksum field itself
        temp = raw.copy()
        del temp["checksum"]
        serialized = json.dumps(temp, sort_keys=True, separators=(",", ":")).encode()
        checksum = hashlib.sha256(serialized).hexdigest()
        
        raw["checksum"] = checksum
        
        try:
            validate_shared_schema(raw, "cognitive_snapshot.schema.json")
        except (SchemaValidationError, OSError, json.JSONDecodeError) as error:
            raise SnapshotError(f"generated snapshot is invalid: {error}") from error
            
        return cls(json.loads(json.dumps(raw, ensure_ascii=False)))

    @classmethod
    def from_mapping(cls, value: dict[str, Any]) -> CognitiveSnapshot:
        try:
            validate_shared_schema(value, "cognitive_snapshot.schema.json")
        except (SchemaValidationError, OSError, json.JSONDecodeError) as error:
            raise SnapshotError(str(error)) from error

        # Validate checksum
        temp = dict(value)
        expected_checksum = temp.pop("checksum")
        serialized = json.dumps(temp, sort_keys=True, separators=(",", ":")).encode()
        actual_checksum = hashlib.sha256(serialized).hexdigest()

        if expected_checksum != actual_checksum:
            raise SnapshotError("snapshot checksum mismatch")

        return cls(json.loads(json.dumps(value, ensure_ascii=False)))

    @property
    def payload(self) -> dict[str, Any]:
        return copy.deepcopy(self.data["payload"])

    @property
    def last_applied_event_id(self) -> str:
        return self.data["last_applied_event_id"]

    @property
    def configuration_fingerprint(self) -> str:
        return self.data["configuration_fingerprint"]
