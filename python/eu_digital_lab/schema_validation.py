"""Small dependency-free validator for the shared JSON Schema subset."""

from __future__ import annotations

import json
import re
from collections.abc import Mapping
from datetime import datetime
from pathlib import Path
from typing import Any


class SchemaValidationError(ValueError):
    """Raised when a value does not satisfy a shared executable schema."""


def validate_shared_schema(value: Any, schema_name: str) -> None:
    schema_path = (
        Path(__file__).resolve().parents[2] / "contracts" / "schemas" / schema_name
    )
    schema = json.loads(schema_path.read_text(encoding="utf-8"))
    _validate(value, schema, schema, "$", schema_path)


def _validate(
    value: Any,
    schema: Mapping[str, Any],
    root: Mapping[str, Any],
    path: str,
    schema_path: Path,
) -> None:
    name = schema_path.name
    reference = schema.get("$ref")
    if reference:
        prefix = "#/$defs/"
        if not isinstance(reference, str):
            raise SchemaValidationError(
                f"{name}: unsupported schema reference {reference!r}"
            )
        if reference.startswith(prefix):
            definition = root.get("$defs", {}).get(reference.removeprefix(prefix))
            if not isinstance(definition, Mapping):
                raise SchemaValidationError(
                    f"{name}: unknown schema definition {reference!r}"
                )
            schema = definition
        else:
            if "#" in reference:
                raise SchemaValidationError(
                    f"{name}: unsupported external fragment {reference!r}"
                )
            target_path = (schema_path.parent / reference).resolve()
            try:
                target = json.loads(target_path.read_text(encoding="utf-8"))
            except (OSError, json.JSONDecodeError) as error:
                raise SchemaValidationError(
                    f"{name}: cannot load schema reference {reference!r}"
                ) from error
            if not isinstance(target, Mapping):
                raise SchemaValidationError(
                    f"{name}: referenced schema must be an object"
                )
            _validate(value, target, target, path, target_path)
            return

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
        isinstance(value, str)
        and "maxLength" in schema
        and len(value) > schema["maxLength"]
    ):
        raise SchemaValidationError(f"{name}{path}: string is too long")
    if isinstance(value, str) and "pattern" in schema:
        pattern = schema["pattern"]
        if not isinstance(pattern, str) or re.fullmatch(pattern, value) is None:
            raise SchemaValidationError(f"{name}{path}: string does not match pattern")
    if isinstance(value, str) and schema.get("format") == "date-time":
        try:
            parsed_datetime = datetime.fromisoformat(value)
        except ValueError as error:
            raise SchemaValidationError(
                f"{name}{path}: string is not an ISO-8601 date-time"
            ) from error
        if parsed_datetime.tzinfo is None:
            raise SchemaValidationError(
                f"{name}{path}: date-time must include a timezone"
            )
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
        if "minProperties" in schema and len(value) < schema["minProperties"]:
            raise SchemaValidationError(f"{name}{path}: object has too few fields")
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
                _validate(
                    value[key], child_schema, root, f"{path}.{key}", schema_path
                )
        additional = schema.get("additionalProperties")
        if isinstance(additional, Mapping):
            for key in set(value) - set(properties):
                _validate(value[key], additional, root, f"{path}.{key}", schema_path)
    elif isinstance(value, list):
        if "minItems" in schema and len(value) < schema["minItems"]:
            raise SchemaValidationError(f"{name}{path}: array has too few items")
        if "maxItems" in schema and len(value) > schema["maxItems"]:
            raise SchemaValidationError(f"{name}{path}: array has too many items")
        if schema.get("uniqueItems"):
            serialized = [
                json.dumps(item, sort_keys=True, separators=(",", ":"))
                for item in value
            ]
            if len(set(serialized)) != len(serialized):
                raise SchemaValidationError(f"{name}{path}: array items are not unique")
        item_schema = schema.get("items")
        if item_schema:
            for index, item in enumerate(value):
                _validate(
                    item, item_schema, root, f"{path}[{index}]", schema_path
                )


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
