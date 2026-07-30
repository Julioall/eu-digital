"""Reference validation for low-risk Windows observation policy."""

from __future__ import annotations

import json
from pathlib import Path
from typing import Any

from .schema_validation import SchemaValidationError, validate_shared_schema


MANDATORY_DENYLIST = frozenset(
    {"keepass", "1password", "bitwarden", "lastpass", "dashlane", "password", "private", "incognito", "inprivate", "tor"}
)


class ObservationPolicyError(ValueError):
    """Raised when a capture policy could expose blocked content."""


def validate_capture_policy(value: dict[str, Any]) -> None:
    try:
        validate_shared_schema(value, "capture_policy.schema.json")
    except (SchemaValidationError, OSError, json.JSONDecodeError) as error:
        raise ObservationPolicyError(str(error)) from error
    denylist = {str(item).casefold() for item in value["denylist"]}
    mandatory = {str(item).casefold() for item in value["mandatory_denylist"]}
    if not MANDATORY_DENYLIST.issubset(mandatory) or not mandatory.issubset(denylist):
        raise ObservationPolicyError("mandatory sensitive-application denylist is incomplete")
    if (value["capture_window_title"] or value["capture_clipboard"]) and not value["allowlist"]:
        raise ObservationPolicyError("textual capture requires an explicit application allowlist")
    if value["redaction_version"] != "length-only-v1":
        raise ObservationPolicyError("unsupported redaction version")


def load_capture_policy(path: str | Path) -> dict[str, Any]:
    try:
        value = json.loads(Path(path).read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise ObservationPolicyError(str(error)) from error
    if not isinstance(value, dict):
        raise ObservationPolicyError("capture policy must be an object")
    validate_capture_policy(value)
    return value
