"""Small dependency-free validator for the shared JSON Schema subset."""

from __future__ import annotations

import json
from collections.abc import Mapping
from pathlib import Path
from typing import Any


class SchemaValidationError(ValueError):
    """Raised when a value does not satisfy a shared executable schema."""


def validate_shared_schema(value: Any, schema_name: str) -> None:
    schema_path = (
        Path(__file__).resolve().parents[2] / "contracts" / "schemas" / schema_name
    )
    schema = json.loads(schema_path.read_text(encoding="utf-8"))
    _validate(value, schema, schema, "$", schema_path.name)


def _validate(
    value: Any, schema: Mapping[str, Any], root: Mapping[str, Any], path: str, name: str
) -> None:
    reference = schema.get("$ref")
    if reference:
        if reference != "#/$defs/requirement":
            raise SchemaValidationError(
                f"{name}: unsupported schema reference {reference!r}"
            )
        schema = root["$defs"]["requirement"]

    if "const" in schema and value != schema["const"]:
        raise SchemaValidationError(
            f"{name}{path}: expected constant {schema['const']!r}"
        )
    if "enum" in schema and value not in schema["enum"]:
        raise SchemaValidationError(f"{name}{path}: value is not in the allowed enum")

    kinds = schema.get("type")
    if isinstance(kinds, str):
        kinds = [kinds]
    if kinds and not any(_is_type(value, kind) for kind in kinds):
        raise SchemaValidationError(f"{name}{path}: invalid type")

    if (
        isinstance(value, str)
        and "minLength" in schema
        and len(value) < schema["minLength"]
    ):
        raise SchemaValidationError(f"{name}{path}: string is too short")
    if (
        isinstance(value, (int, float))
        and not isinstance(value, bool)
        and "minimum" in schema
        and value < schema["minimum"]
    ):
        raise SchemaValidationError(f"{name}{path}: value is below minimum")
    if (
        isinstance(value, (int, float))
        and not isinstance(value, bool)
        and "maximum" in schema
        and value > schema["maximum"]
    ):
        raise SchemaValidationError(f"{name}{path}: value is above maximum")

    if isinstance(value, Mapping):
        required = set(schema.get("required", []))
        missing = required - set(value)
        if missing:
            raise SchemaValidationError(
                f"{name}{path}: missing fields {sorted(missing)}"
            )
        properties = schema.get("properties", {})
        if schema.get("additionalProperties") is False:
            extra = set(value) - set(properties)
            if extra:
                raise SchemaValidationError(
                    f"{name}{path}: unknown fields {sorted(extra)}"
                )
        for key, child_schema in properties.items():
            if key in value:
                _validate(value[key], child_schema, root, f"{path}.{key}", name)
    elif isinstance(value, list):
        item_schema = schema.get("items")
        if item_schema:
            for index, item in enumerate(value):
                _validate(item, item_schema, root, f"{path}[{index}]", name)


def _is_type(value: Any, kind: str) -> bool:
    return {
        "object": isinstance(value, Mapping),
        "array": isinstance(value, list),
        "string": isinstance(value, str),
        "integer": isinstance(value, int) and not isinstance(value, bool),
        "number": isinstance(value, (int, float)) and not isinstance(value, bool),
        "boolean": isinstance(value, bool),
        "null": value is None,
    }.get(kind, False)
