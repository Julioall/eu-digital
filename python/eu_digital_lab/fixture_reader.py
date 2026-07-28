"""Read the shared CanonicalEvent fixture without duplicating its schema."""

from __future__ import annotations

import json
from pathlib import Path
from typing import Any


def read_canonical_event(path: str | Path, schema_path: str | Path | None = None) -> dict[str, Any]:
    fixture_path = Path(path)
    if schema_path is None:
        schema_path = fixture_path.parents[1] / "schemas" / "canonical_event.schema.json"
    schema = json.loads(Path(schema_path).read_text(encoding="utf-8"))
    value = json.loads(fixture_path.read_text(encoding="utf-8"))
    if not isinstance(value, dict):
        raise ValueError("CanonicalEvent fixture must be an object")
    missing = set(schema["required"]) - set(value)
    if missing:
        raise ValueError(f"CanonicalEvent fixture missing fields: {sorted(missing)}")
    if value["schema_version"] != schema["properties"]["schema_version"]["const"]:
        raise ValueError("unsupported CanonicalEvent schema version")
    return value
