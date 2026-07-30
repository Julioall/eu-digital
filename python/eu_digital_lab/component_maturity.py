"""Separate component maturity state from SPEC and promotion status."""

from __future__ import annotations

import json
from collections.abc import Mapping
from dataclasses import dataclass
from pathlib import Path
from typing import Any

from .schema_validation import SchemaValidationError, validate_shared_schema


class ComponentMaturityError(ValueError):
    """Raised when a component maturity registry is inconsistent."""


@dataclass(frozen=True)
class ComponentMaturityRegistry:
    data: Mapping[str, Any]

    @classmethod
    def load(cls, path: str | Path) -> ComponentMaturityRegistry:
        try:
            value = json.loads(Path(path).read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError) as error:
            raise ComponentMaturityError(f"cannot load component maturity registry: {error}") from error
        return cls.from_dict(value)

    @classmethod
    def from_dict(cls, value: Mapping[str, Any]) -> ComponentMaturityRegistry:
        if not isinstance(value, Mapping):
            raise ComponentMaturityError("component maturity registry must be an object")
        copied = json.loads(json.dumps(value, ensure_ascii=False))
        registry = cls(copied)
        registry.validate()
        return registry

    def validate(self) -> None:
        try:
            validate_shared_schema(self.data, "component_maturity.schema.json")
        except SchemaValidationError as error:
            raise ComponentMaturityError(str(error)) from error

        components = self.data["components"]
        if not isinstance(components, list) or not components:
            raise ComponentMaturityError("component maturity registry must contain components")
        component_ids: set[str] = set()
        for component in components:
            component_id = component["component_id"]
            if component_id in component_ids:
                raise ComponentMaturityError(f"duplicate component_id: {component_id}")
            component_ids.add(component_id)
            reference = component["reference_status"]
            native = component["native_status"]
            product = component["product_status"]
            promotion_id = component["promotion_id"]
            if native in {"equivalent", "promoted"} and reference != "frozen":
                raise ComponentMaturityError(
                    f"{component_id}: native {native} requires frozen reference"
                )
            if native == "promoted" and not promotion_id:
                raise ComponentMaturityError(
                    f"{component_id}: promoted native component requires promotion_id"
                )
            if product in {"beta", "released"} and native != "promoted":
                raise ComponentMaturityError(
                    f"{component_id}: product {product} requires promoted native component"
                )
            if product == "released" and not promotion_id:
                raise ComponentMaturityError(
                    f"{component_id}: released component requires promotion_id"
                )

    def validate_spec_references(self, specs_path: str | Path) -> None:
        specs = Path(specs_path)
        for component in self.data["components"]:
            spec_id = component["spec_id"]
            matches = tuple(specs.glob(f"{spec_id}-*.md"))
            if len(matches) != 1:
                raise ComponentMaturityError(
                    f"{component['component_id']}: SPEC reference is not unique: {spec_id}"
                )

    def validate_evidence_references(self, repository_root: str | Path) -> None:
        root = Path(repository_root).resolve()
        for component in self.data["components"]:
            for evidence_ref in component["evidence_refs"]:
                evidence = Path(evidence_ref)
                if evidence.is_absolute() or ".." in evidence.parts:
                    raise ComponentMaturityError(
                        f"{component['component_id']}: evidence reference escapes repository: {evidence_ref}"
                    )
                resolved = (root / evidence).resolve()
                if not resolved.is_relative_to(root) or not resolved.is_file():
                    raise ComponentMaturityError(
                        f"{component['component_id']}: evidence reference does not exist: {evidence_ref}"
                    )

    def component(self, component_id: str) -> Mapping[str, Any]:
        for component in self.data["components"]:
            if component["component_id"] == component_id:
                return component
        raise ComponentMaturityError(f"component is not registered: {component_id}")
