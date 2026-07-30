"""Reference validation for local consent and storage governance contracts."""

from __future__ import annotations

import json
from dataclasses import dataclass
from pathlib import Path
from typing import Any

from .schema_validation import SchemaValidationError, validate_shared_schema


class PrivacyStorageError(ValueError):
    """Raised when a privacy or storage contract is inconsistent."""


def _validate(value: Any, schema_name: str) -> None:
    try:
        validate_shared_schema(value, schema_name)
    except (SchemaValidationError, OSError, json.JSONDecodeError) as error:
        raise PrivacyStorageError(str(error)) from error


@dataclass(frozen=True)
class ConsentPolicy:
    data: dict[str, Any]

    @classmethod
    def from_mapping(cls, value: dict[str, Any]) -> ConsentPolicy:
        _validate(value, "consent_policy.schema.json")
        seen: set[tuple[str, str, str]] = set()
        for consent in value["consents"]:
            key = (consent["sensor_id"], consent["purpose"], consent["decided_at"])
            if key in seen:
                raise PrivacyStorageError("duplicate consent decision")
            seen.add(key)
        return cls(json.loads(json.dumps(value, ensure_ascii=False)))

    @classmethod
    def load(cls, path: str | Path) -> ConsentPolicy:
        try:
            value = json.loads(Path(path).read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError) as error:
            raise PrivacyStorageError(str(error)) from error
        if not isinstance(value, dict):
            raise PrivacyStorageError("consent policy must be an object")
        return cls.from_mapping(value)

    def allows(self, sensor_id: str, purpose: str) -> bool:
        if self.data["global_pause"]:
            return False
        for consent in reversed(self.data["consents"]):
            if consent["sensor_id"] == sensor_id and consent["purpose"] == purpose:
                return consent["decision"] == "grant"
        return False


@dataclass(frozen=True)
class StoragePolicy:
    data: dict[str, Any]

    @classmethod
    def from_mapping(cls, value: dict[str, Any]) -> StoragePolicy:
        _validate(value, "storage_policy.schema.json")
        expected_buckets = {
            "database",
            "wal",
            "indexes",
            "quarantine",
            "backups",
            "payloads",
        }
        buckets = value["included_buckets"]
        if set(buckets) != expected_buckets or len(buckets) != len(expected_buckets):
            raise PrivacyStorageError("storage policy must count all user-data buckets exactly once")
        if value["model_accounting"] != "separate":
            raise PrivacyStorageError("model storage must be accounted separately")
        if value["quota_exceeded_action"] != "suspend_capture_and_request_user_decision":
            raise PrivacyStorageError("quota overflow cannot delete data automatically")
        return cls(json.loads(json.dumps(value, ensure_ascii=False)))

    @classmethod
    def load(cls, path: str | Path) -> StoragePolicy:
        try:
            value = json.loads(Path(path).read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError) as error:
            raise PrivacyStorageError(str(error)) from error
        if not isinstance(value, dict):
            raise PrivacyStorageError("storage policy must be an object")
        return cls.from_mapping(value)


def validate_storage_health(value: dict[str, Any]) -> None:
    _validate(value, "storage_health.schema.json")
    if value["status"] == "ready" and (
        value["capture_suspended"] or value["user_decision_required"] or value["reason_code"]
    ):
        raise PrivacyStorageError("ready storage health cannot report a blocking condition")
    if value["reason_code"] == "storage_quota_exceeded" and not (
        value["status"] == "degraded"
        and value["capture_suspended"]
        and value["user_decision_required"]
    ):
        raise PrivacyStorageError("quota overflow must suspend capture and request a decision")
    if value["user_bytes"] > value["quota_bytes"] and value["status"] != "degraded":
        raise PrivacyStorageError("storage over quota must be degraded")


def validate_data_management_request(value: dict[str, Any]) -> None:
    _validate(value, "data_management_request.schema.json")
    if not value["confirmed"]:
        raise PrivacyStorageError("data export and deletion require explicit confirmation")


def validate_privacy_storage_fixtures(root: str | Path) -> None:
    repository = Path(root)
    ConsentPolicy.load(repository / "contracts/fixtures/consent_policy.json")
    StoragePolicy.load(repository / "contracts/fixtures/storage_policy.json")
    health = json.loads(
        (repository / "contracts/fixtures/storage_health.json").read_text(encoding="utf-8")
    )
    validate_storage_health(health)
    request = json.loads(
        (repository / "contracts/fixtures/data_management_request.json").read_text(encoding="utf-8")
    )
    validate_data_management_request(request)
