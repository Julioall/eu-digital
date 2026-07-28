"""Read the shared CanonicalEvent fixture without duplicating its schema."""

from __future__ import annotations

import json
from pathlib import Path
from typing import Any, Mapping


def _schema_path(schema_path: str | Path | None) -> Path:
    if schema_path is not None:
        return Path(schema_path)
    return Path(__file__).resolve().parents[2] / "contracts" / "schemas" / "canonical_event.schema.json"


def validate_canonical_event(
    value: Mapping[str, Any], schema_path: str | Path | None = None
) -> None:
    """Validate the bus envelope using the executable shared JSON schema."""
    if not isinstance(value, Mapping):
        raise ValueError("CanonicalEvent must be an object")
    schema = json.loads(_schema_path(schema_path).read_text(encoding="utf-8"))
    missing = set(schema["required"]) - set(value)
    if missing:
        raise ValueError(f"CanonicalEvent missing fields: {sorted(missing)}")
    for name, definition in schema["properties"].items():
        if name not in value:
            continue
        current = value[name]
        if "const" in definition and current != definition["const"]:
            raise ValueError(f"CanonicalEvent field {name!r} must equal {definition['const']!r}")
        types = definition.get("type", [])
        if isinstance(types, str):
            types = [types]
        valid = any(
            (kind == "string" and isinstance(current, str))
            or (kind == "object" and isinstance(current, Mapping))
            or (kind == "array" and isinstance(current, list))
            or (kind == "integer" and isinstance(current, int) and not isinstance(current, bool))
            or (kind == "null" and current is None)
            for kind in types
        )
        if not valid:
            raise ValueError(f"CanonicalEvent field {name!r} has invalid type")
        if definition.get("minimum") is not None and current < definition["minimum"]:
            raise ValueError(f"CanonicalEvent field {name!r} is below its minimum")
    extra = set(value) - set(schema["properties"])
    if extra and schema.get("additionalProperties") is False:
        raise ValueError(f"CanonicalEvent has unknown fields: {sorted(extra)}")


def read_canonical_event(path: str | Path, schema_path: str | Path | None = None) -> dict[str, Any]:
    fixture_path = Path(path)
    value = json.loads(fixture_path.read_text(encoding="utf-8"))
    validate_canonical_event(value, schema_path or fixture_path.parents[1] / "schemas" / "canonical_event.schema.json")
    return value
