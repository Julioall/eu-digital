"""Removable capability runtime for the local research environment.

The module only knows versioned capability contracts. Concrete integrations are
loaded by discovery and are never imported by the runtime core.
"""

from __future__ import annotations

import copy
import hashlib
import importlib
import importlib.metadata
import inspect
import json
import time
import uuid
from collections.abc import Callable, Iterable, Mapping, Sequence
from dataclasses import dataclass, replace
from datetime import UTC, datetime
from pathlib import Path
from typing import Any

from .event_bus import AsyncEventBus
from .schema_validation import SchemaValidationError, validate_shared_schema


class CapabilityRuntimeError(RuntimeError):
    """Base class for typed capability runtime failures."""


class ManifestValidationError(CapabilityRuntimeError):
    """A descriptor or local plugin manifest is invalid."""


class NoCapabilityProviderError(CapabilityRuntimeError):
    """No available capability provides the requested operation."""


class LifecycleError(CapabilityRuntimeError):
    """A lifecycle transition or plugin operation failed."""


class CheckpointUnavailableError(CapabilityRuntimeError):
    """A stateful plugin could not provide or restore a checkpoint."""


@dataclass(frozen=True)
class CapabilityOperation:
    operation: str
    input_schema: str | None
    output_schema: str
    modalities: tuple[str, ...]
    side_effect: str
    streaming: bool

    @classmethod
    def from_mapping(cls, value: Mapping[str, Any]) -> CapabilityOperation:
        return cls(
            operation=str(value["operation"]),
            input_schema=value["input_schema"],
            output_schema=str(value["output_schema"]),
            modalities=tuple(str(item) for item in value["modalities"]),
            side_effect=str(value["side_effect"]),
            streaming=bool(value["streaming"]),
        )

    def to_mapping(self) -> dict[str, Any]:
        return {
            "operation": self.operation,
            "input_schema": self.input_schema,
            "output_schema": self.output_schema,
            "modalities": list(self.modalities),
            "side_effect": self.side_effect,
            "streaming": self.streaming,
        }


@dataclass(frozen=True)
class CapabilityRequirement:
    operation: str
    version_range: str | None

    @classmethod
    def from_mapping(cls, value: Mapping[str, Any]) -> CapabilityRequirement:
        return cls(str(value["operation"]), value["version_range"])

    def to_mapping(self) -> dict[str, Any]:
        return {"operation": self.operation, "version_range": self.version_range}


@dataclass(frozen=True)
class CapabilityDescriptor:
    schema_version: str
    capability_id: str
    implementation_id: str
    implementation_version: str
    kind: str
    provides: tuple[CapabilityOperation, ...]
    mandatory: tuple[CapabilityRequirement, ...]
    optional: tuple[CapabilityRequirement, ...]
    execution: str
    startup: str
    supports_hot_plug: bool
    supports_checkpoint: bool
    estimated_ram_mb: int | None
    estimated_cpu_class: str
    confidence_model: str | None
    latency_class: str
    calibration_required: bool
    permissions: tuple[str, ...]
    config_schema: str | None
    health_check_operation: str

    @classmethod
    def from_mapping(cls, value: Mapping[str, Any]) -> CapabilityDescriptor:
        try:
            validate_shared_schema(value, "capability_descriptor.schema.json")
        except (SchemaValidationError, OSError, json.JSONDecodeError) as exc:
            raise ManifestValidationError(str(exc)) from exc
        requires = value["requires"]
        runtime = value["runtime"]
        quality = value["quality"]
        return cls(
            schema_version=str(value["schema_version"]),
            capability_id=str(value["capability_id"]),
            implementation_id=str(value["implementation_id"]),
            implementation_version=str(value["implementation_version"]),
            kind=str(value["kind"]),
            provides=tuple(
                CapabilityOperation.from_mapping(item) for item in value["provides"]
            ),
            mandatory=tuple(
                CapabilityRequirement.from_mapping(item)
                for item in requires["mandatory"]
            ),
            optional=tuple(
                CapabilityRequirement.from_mapping(item)
                for item in requires["optional"]
            ),
            execution=str(runtime["execution"]),
            startup=str(runtime["startup"]),
            supports_hot_plug=bool(runtime["supports_hot_plug"]),
            supports_checkpoint=bool(runtime["supports_checkpoint"]),
            estimated_ram_mb=runtime["estimated_ram_mb"],
            estimated_cpu_class=str(runtime["estimated_cpu_class"]),
            confidence_model=quality["confidence_model"],
            latency_class=str(quality["latency_class"]),
            calibration_required=bool(quality["calibration_required"]),
            permissions=tuple(str(item) for item in value["permissions"]),
            config_schema=value["config_schema"],
            health_check_operation=str(value["health_check_operation"]),
        )

    def to_mapping(self) -> dict[str, Any]:
        return {
            "schema_version": self.schema_version,
            "capability_id": self.capability_id,
            "implementation_id": self.implementation_id,
            "implementation_version": self.implementation_version,
            "kind": self.kind,
            "provides": [item.to_mapping() for item in self.provides],
            "requires": {
                "mandatory": [item.to_mapping() for item in self.mandatory],
                "optional": [item.to_mapping() for item in self.optional],
            },
            "runtime": {
                "execution": self.execution,
                "startup": self.startup,
                "supports_hot_plug": self.supports_hot_plug,
                "supports_checkpoint": self.supports_checkpoint,
                "estimated_ram_mb": self.estimated_ram_mb,
                "estimated_cpu_class": self.estimated_cpu_class,
            },
            "quality": {
                "confidence_model": self.confidence_model,
                "latency_class": self.latency_class,
                "calibration_required": self.calibration_required,
            },
            "permissions": list(self.permissions),
            "config_schema": self.config_schema,
            "health_check_operation": self.health_check_operation,
        }

    def supports(self, operation: str) -> bool:
        return any(item.operation == operation for item in self.provides)


class CapabilityPlugin:
    """Lifecycle port implemented by a removable plugin."""

    descriptor: CapabilityDescriptor

    def validate_manifest(self) -> None:
        return None

    def configure(self, _config: Mapping[str, Any] | None = None) -> None:
        return None

    def initialize(self) -> None:
        return None

    def calibrate(self) -> None:
        return None

    def health_check(self) -> bool:
        return True

    def start(self) -> None:
        return None

    def pause(self) -> None:
        return None

    def resume(self) -> None:
        return None

    def drain(self) -> None:
        return None

    def checkpoint(self) -> Mapping[str, Any] | None:
        return None

    def restore_checkpoint(self, _value: Mapping[str, Any]) -> None:
        return None

    def stop(self) -> None:
        return None

    def uninstall(self) -> None:
        return None


@dataclass(frozen=True)
class PluginManifest:
    descriptor: CapabilityDescriptor
    entry_point: str
    priority: int = 0

    @classmethod
    def from_mapping(cls, value: Mapping[str, Any]) -> PluginManifest:
        try:
            validate_shared_schema(value, "plugin_manifest.schema.json")
        except (SchemaValidationError, OSError, json.JSONDecodeError) as exc:
            raise ManifestValidationError(str(exc)) from exc
        return cls(
            descriptor=CapabilityDescriptor.from_mapping(value["descriptor"]),
            entry_point=str(value["entry_point"]),
            priority=int(value["priority"]),
        )


PluginFactory = Callable[[], CapabilityPlugin]


@dataclass(frozen=True)
class DiscoveredPlugin:
    manifest: PluginManifest
    factory: PluginFactory

    @property
    def descriptor(self) -> CapabilityDescriptor:
        return self.manifest.descriptor


class PluginDiscovery:
    """Discover local JSON manifests and installed Python entry points."""

    def __init__(
        self,
        *,
        group: str = "eu_digital.capabilities",
        entry_points_provider: Callable[[str], Iterable[Any]] | None = None,
        entry_point_loader: Callable[[str], Any] | None = None,
    ) -> None:
        self.group = group
        self._entry_points_provider = entry_points_provider
        self._entry_point_loader = entry_point_loader or self._load_reference

    def discover(
        self, manifest_dir: str | Path | None = None
    ) -> tuple[DiscoveredPlugin, ...]:
        discovered: list[DiscoveredPlugin] = []
        if manifest_dir is not None:
            directory = Path(manifest_dir)
            for path in sorted(directory.glob("*.json")):
                value = json.loads(path.read_text(encoding="utf-8"))
                manifest = PluginManifest.from_mapping(value)
                discovered.append(
                    DiscoveredPlugin(manifest, self._factory(manifest.entry_point))
                )

        provider = self._entry_points_provider or self._installed_entry_points
        for entry_point in provider(self.group):
            loaded = entry_point.load()
            factory = self._factory_from_loaded(loaded)
            plugin = factory()
            descriptor = CapabilityDescriptor.from_mapping(
                plugin.descriptor.to_mapping()
            )
            manifest = PluginManifest(
                descriptor, getattr(entry_point, "value", entry_point.name), 0
            )
            discovered.append(DiscoveredPlugin(manifest, factory))
        return tuple(discovered)

    def _installed_entry_points(self, group: str) -> Iterable[Any]:
        points = importlib.metadata.entry_points()
        if hasattr(points, "select"):
            return points.select(group=group)
        return tuple(
            point for point in points if getattr(point, "group", None) == group
        )

    def _factory(self, reference: str) -> PluginFactory:
        return self._factory_from_loaded(self._entry_point_loader(reference))

    @staticmethod
    def _load_reference(reference: str) -> Any:
        try:
            module_name, attribute = reference.split(":", 1)
            return getattr(importlib.import_module(module_name), attribute)
        except (ValueError, ImportError, AttributeError) as exc:
            raise ManifestValidationError(
                f"invalid plugin entry point {reference!r}"
            ) from exc

    @staticmethod
    def _factory_from_loaded(loaded: Any) -> PluginFactory:
        if isinstance(loaded, CapabilityPlugin):
            return lambda: loaded
        if not callable(loaded):
            raise ManifestValidationError(
                "plugin entry point must load a factory or CapabilityPlugin"
            )
        return loaded


@dataclass(frozen=True)
class CapabilityState:
    capability_id: str
    implementation_id: str
    state: str
    changed_at: str
    reason_code: str | None = None
    message: str | None = None
    health_score: float | None = None
    consecutive_failures: int = 0
    blocked_goal_ids: tuple[str, ...] = ()

    def to_mapping(self) -> dict[str, Any]:
        return {
            "capability_id": self.capability_id,
            "implementation_id": self.implementation_id,
            "state": self.state,
            "changed_at": self.changed_at,
            "reason_code": self.reason_code,
            "message": self.message,
            "health": {
                "score": self.health_score,
                "last_check_at": self.changed_at,
                "consecutive_failures": self.consecutive_failures,
            },
            "performance": {
                "latency_ms_p50": None,
                "latency_ms_p95": None,
                "success_rate": None,
                "confidence_calibration": None,
            },
            "availability": {
                "since": self.changed_at if self.state == "available" else None,
                "expected_recovery_at": None,
            },
            "impact": {
                "affected_operations": [],
                "blocked_goal_ids": list(self.blocked_goal_ids),
                "confidence_adjustment": None,
            },
        }

    @classmethod
    def from_mapping(cls, value: Mapping[str, Any]) -> CapabilityState:
        health = value.get("health", {})
        impact = value.get("impact", {})
        return cls(
            capability_id=str(value["capability_id"]),
            implementation_id=str(value["implementation_id"]),
            state=str(value["state"]),
            changed_at=str(value["changed_at"]),
            reason_code=value.get("reason_code"),
            message=value.get("message"),
            health_score=health.get("score"),
            consecutive_failures=int(health.get("consecutive_failures", 0)),
            blocked_goal_ids=tuple(
                str(item) for item in impact.get("blocked_goal_ids", [])
            ),
        )


@dataclass
class CapabilityRecord:
    descriptor: CapabilityDescriptor
    state: CapabilityState
    priority: int = 0
    checkpoint: dict[str, Any] | None = None


class FunctionalSelfModel:
    def __init__(self, owner_user_id: str = "local-user") -> None:
        self._snapshot: dict[str, Any] = {
            "self_model_version": 0,
            "identity": {
                "name": "eu-digital",
                "role": "local cognitive agent",
                "owner_user_id": owner_user_id,
            },
            "capabilities": {
                "available": [],
                "degraded": [],
                "temporarily_unavailable": [],
                "disabled": [],
                "removed": [],
                "potential": [],
                "capability_history": [],
            },
            "observability": {
                "available_modalities": [],
                "unavailable_modalities": [],
                "known_blind_spots": [],
            },
            "sensors": {"active": [], "degraded": []},
            "state": {
                "mode": "observing",
                "active_goal_ids": [],
                "workspace_item_ids": [],
            },
            "knowledge": {"learned_pattern_count": 0, "unresolved_contradictions": 0},
            "confidence": {"global": 1.0, "calibration_score": None},
            "continuity": {
                "created_at": _now(),
                "last_updated_at": _now(),
                "prior_version_hash": None,
            },
        }

    @property
    def snapshot(self) -> dict[str, Any]:
        return copy.deepcopy(self._snapshot)

    def restore(self, value: Mapping[str, Any]) -> None:
        self._snapshot = copy.deepcopy(dict(value))

    def update(
        self, records: Iterable[CapabilityRecord], blocked_plans: Sequence[str]
    ) -> None:
        previous = json.dumps(
            self._snapshot, sort_keys=True, separators=(",", ":")
        ).encode()
        records_list = list(records)
        by_state: dict[str, list[str]] = {
            state: []
            for state in (
                "available",
                "degraded",
                "temporarily_unavailable",
                "disabled",
                "removed",
            )
        }
        available_modalities: set[str] = set()
        unavailable_modalities: set[str] = set()
        active_sensors: list[str] = []
        degraded_sensors: list[str] = []
        history: set[str] = set(
            self._snapshot["capabilities"].get("capability_history", [])
        )
        for record in records_list:
            implementation_id = record.descriptor.implementation_id
            history.add(implementation_id)
            if record.state.state in by_state:
                by_state[record.state.state].append(implementation_id)
            modalities = {
                modality
                for operation in record.descriptor.provides
                for modality in operation.modalities
            }
            if record.state.state in {"available", "degraded"}:
                available_modalities.update(modalities)
                if record.descriptor.kind == "sensor":
                    (
                        degraded_sensors
                        if record.state.state == "degraded"
                        else active_sensors
                    ).append(implementation_id)
            else:
                unavailable_modalities.update(modalities)
        self._snapshot["self_model_version"] += 1
        self._snapshot["capabilities"] = {
            **self._snapshot["capabilities"],
            **{key: sorted(value) for key, value in by_state.items()},
            "potential": [],
            "capability_history": sorted(history),
        }
        self._snapshot["observability"] = {
            "available_modalities": sorted(available_modalities),
            "unavailable_modalities": sorted(
                unavailable_modalities - available_modalities
            ),
            "known_blind_spots": sorted(unavailable_modalities - available_modalities),
        }
        self._snapshot["sensors"] = {
            "active": sorted(active_sensors),
            "degraded": sorted(degraded_sensors),
        }
        self._snapshot["state"] = {
            **self._snapshot["state"],
            "mode": "degraded"
            if any(
                record.state.state
                in {"degraded", "temporarily_unavailable", "failed", "incompatible"}
                for record in records_list
            )
            else "observing",
            "active_goal_ids": sorted(
                set(self._snapshot["state"].get("active_goal_ids", []))
                - set(blocked_plans)
            ),
        }
        self._snapshot["continuity"] = {
            **self._snapshot["continuity"],
            "last_updated_at": _now(),
            "prior_version_hash": hashlib.sha256(previous).hexdigest(),
        }


class CapabilityRegistry:
    """Persistent source of truth for descriptors, states and checkpoints."""

    def __init__(self, path: str | Path | None = None) -> None:
        self.path = Path(path) if path is not None else None
        self._records: dict[str, CapabilityRecord] = {}
        self._plans: dict[str, set[str]] = {}
        self._blocked_plans: set[str] = set()
        self._profiles: dict[str, set[str]] = {}
        self._active_profile: str | None = None
        self.self_model = FunctionalSelfModel()
        if self.path is not None and self.path.exists():
            self._load()

    def records(self) -> tuple[CapabilityRecord, ...]:
        return tuple(self._records[key] for key in sorted(self._records))

    @property
    def blocked_plans(self) -> tuple[str, ...]:
        return tuple(sorted(self._blocked_plans))

    @property
    def active_profile(self) -> str | None:
        return self._active_profile

    def define_profile(
        self, profile_id: str, implementation_ids: Iterable[str]
    ) -> None:
        self._profiles[profile_id] = set(implementation_ids)
        self.save()

    def activate_profile(self, profile_id: str | None) -> None:
        if profile_id is not None and profile_id not in self._profiles:
            raise LifecycleError(f"unknown capability profile: {profile_id}")
        self._active_profile = profile_id
        self.save()

    def allows_in_profile(self, implementation_id: str) -> bool:
        return (
            self._active_profile is None
            or implementation_id in self._profiles[self._active_profile]
        )

    def state(self, implementation_id: str) -> CapabilityState:
        return self._records[implementation_id].state

    def record(self, implementation_id: str) -> CapabilityRecord:
        return self._records[implementation_id]

    def add(
        self, descriptor: CapabilityDescriptor, priority: int = 0
    ) -> CapabilityRecord:
        existing = self._records.get(descriptor.implementation_id)
        if existing is not None and existing.state.state != "removed":
            raise LifecycleError(
                f"implementation already registered: {descriptor.implementation_id}"
            )
        state = _state(descriptor, "unknown")
        record = CapabilityRecord(
            descriptor, state, priority, existing.checkpoint if existing else None
        )
        self._records[descriptor.implementation_id] = record
        self._refresh_model()
        self.save()
        return record

    def set_state(
        self,
        implementation_id: str,
        state: str,
        *,
        reason_code: str | None = None,
        message: str | None = None,
        health_score: float | None = None,
    ) -> CapabilityState:
        record = self._records[implementation_id]
        allowed = {
            "unknown": {"discovered", "incompatible", "removed"},
            "discovered": {"calibrating", "incompatible", "failed", "removed"},
            "calibrating": {
                "available",
                "failed",
                "temporarily_unavailable",
                "removed",
            },
            "available": {
                "degraded",
                "temporarily_unavailable",
                "disabled",
                "failed",
                "removed",
            },
            "degraded": {
                "available",
                "temporarily_unavailable",
                "disabled",
                "failed",
                "removed",
            },
            "temporarily_unavailable": {
                "calibrating",
                "available",
                "disabled",
                "failed",
                "removed",
            },
            "disabled": {"calibrating", "available", "removed"},
            "failed": {"calibrating", "temporarily_unavailable", "removed"},
            "incompatible": {"discovered", "removed"},
            "removed": {"discovered"},
        }
        if state != record.state.state and state not in allowed.get(
            record.state.state, set()
        ):
            raise LifecycleError(
                f"invalid capability transition {record.state.state!r} -> {state!r}"
            )
        record.state = replace(
            record.state,
            state=state,
            changed_at=_now(),
            reason_code=reason_code,
            message=message,
            health_score=health_score,
        )
        self._refresh_model()
        self.save()
        return record.state

    def set_checkpoint(
        self, implementation_id: str, value: Mapping[str, Any] | None
    ) -> None:
        self._records[implementation_id].checkpoint = (
            copy.deepcopy(dict(value)) if value is not None else None
        )
        self.save()

    def register_plan(
        self, plan_id: str, required_implementation_ids: Iterable[str]
    ) -> None:
        self._plans[plan_id] = set(required_implementation_ids)
        self.save()

    def invalidate_for(self, implementation_id: str) -> tuple[str, ...]:
        affected = tuple(
            sorted(
                plan_id
                for plan_id, required in self._plans.items()
                if implementation_id in required
            )
        )
        self._blocked_plans.update(affected)
        self._refresh_model()
        self.save()
        return affected

    def _refresh_model(self) -> None:
        self.self_model.update(self.records(), self.blocked_plans)

    def save(self) -> None:
        if self.path is None:
            return
        self.path.parent.mkdir(parents=True, exist_ok=True)
        value = {
            "schema_version": "1.0",
            "records": {
                implementation_id: {
                    "descriptor": record.descriptor.to_mapping(),
                    "state": record.state.to_mapping(),
                    "priority": record.priority,
                    "checkpoint": record.checkpoint,
                }
                for implementation_id, record in sorted(self._records.items())
            },
            "plans": {
                plan_id: sorted(required)
                for plan_id, required in sorted(self._plans.items())
            },
            "blocked_plans": sorted(self._blocked_plans),
            "profiles": {
                profile_id: sorted(implementation_ids)
                for profile_id, implementation_ids in sorted(self._profiles.items())
            },
            "active_profile": self._active_profile,
            "self_model": self.self_model.snapshot,
        }
        self.path.write_text(
            json.dumps(value, indent=2, sort_keys=True) + "\n", encoding="utf-8"
        )

    def _load(self) -> None:
        path = self.path
        if path is None:
            raise ManifestValidationError("registry path is required for restore")
        value = json.loads(path.read_text(encoding="utf-8"))
        for implementation_id, raw in value.get("records", {}).items():
            descriptor = CapabilityDescriptor.from_mapping(raw["descriptor"])
            state = CapabilityState.from_mapping(raw["state"])
            if (
                implementation_id != descriptor.implementation_id
                or implementation_id != state.implementation_id
            ):
                raise ManifestValidationError(
                    "registry implementation identifiers do not agree"
                )
            self._records[implementation_id] = CapabilityRecord(
                descriptor, state, int(raw.get("priority", 0)), raw.get("checkpoint")
            )
        self._plans = {
            str(plan_id): {str(item) for item in required}
            for plan_id, required in value.get("plans", {}).items()
        }
        self._blocked_plans = {str(item) for item in value.get("blocked_plans", [])}
        self._profiles = {
            str(profile_id): {str(item) for item in implementation_ids}
            for profile_id, implementation_ids in value.get("profiles", {}).items()
        }
        self._active_profile = value.get("active_profile")
        if value.get("self_model"):
            self.self_model.restore(value["self_model"])
        else:
            self._refresh_model()


@dataclass(frozen=True)
class Resolution:
    operation: str
    implementation_id: str
    fallback: bool
    reason: str


class CapabilityRuntime:
    """Coordinates registry, plugin lifecycle, event audit and resolution."""

    def __init__(
        self,
        event_bus: AsyncEventBus,
        registry_path: str | Path | None = None,
        *,
        allowed_permissions: Iterable[str] = (),
    ) -> None:
        self.event_bus = event_bus
        self.registry = CapabilityRegistry(registry_path)
        self.self_model = self.registry.self_model
        self._plugins: dict[str, CapabilityPlugin] = {}
        self._session_id = str(uuid.uuid4())
        self._allowed_permissions = frozenset(allowed_permissions)

    def discovery_from_plugin(
        self, plugin: CapabilityPlugin, priority: int = 0
    ) -> DiscoveredPlugin:
        descriptor = CapabilityDescriptor.from_mapping(plugin.descriptor.to_mapping())
        return DiscoveredPlugin(
            PluginManifest(descriptor, "in-process:test", priority), lambda: plugin
        )

    async def install(
        self, discovered: DiscoveredPlugin, config: Mapping[str, Any] | None = None
    ) -> CapabilityState:
        descriptor = CapabilityDescriptor.from_mapping(
            discovered.descriptor.to_mapping()
        )
        plugin = discovered.factory()
        plugin_descriptor = CapabilityDescriptor.from_mapping(
            plugin.descriptor.to_mapping()
        )
        if plugin_descriptor.implementation_id != descriptor.implementation_id:
            raise ManifestValidationError(
                "manifest and plugin implementation IDs do not agree"
            )
        self.registry.add(descriptor, discovered.manifest.priority)
        self._plugins[descriptor.implementation_id] = plugin
        try:
            await self._transition(descriptor.implementation_id, "discovered")
            self._check_dependencies(descriptor)
            missing_permissions = (
                set(descriptor.permissions) - self._allowed_permissions
            )
            if missing_permissions:
                raise LifecycleError(
                    f"permissions not granted: {sorted(missing_permissions)}"
                )
            plugin.validate_manifest()
            await self._transition(descriptor.implementation_id, "calibrating")
            await _invoke(plugin.configure, config)
            await _invoke(plugin.initialize)
            await _invoke(plugin.calibrate)
            healthy = await _invoke(plugin.health_check)
            if healthy is False:
                raise LifecycleError("health check failed")
            await self._transition(
                descriptor.implementation_id, "available", health_score=1.0
            )
            await _invoke(plugin.start)
            return self.registry.state(descriptor.implementation_id)
        except Exception as exc:  # noqa: BLE001 - plugin failures must be isolated
            reason_code = (
                "missing_dependency"
                if isinstance(exc, LifecycleError)
                and str(exc).startswith("missing mandatory dependency")
                else "plugin_initialization_failed"
            )
            await self._transition(
                descriptor.implementation_id,
                "failed",
                reason_code=reason_code,
                message=str(exc),
            )
            return self.registry.state(descriptor.implementation_id)

    async def restore(self, discovered: Iterable[DiscoveredPlugin]) -> None:
        for item in discovered:
            implementation_id = item.descriptor.implementation_id
            if implementation_id not in {
                record.descriptor.implementation_id
                for record in self.registry.records()
            }:
                continue
            record = self.registry.record(implementation_id)
            if record.state.state == "removed":
                continue
            plugin = item.factory()
            self._plugins[implementation_id] = plugin
            try:
                await _invoke(plugin.validate_manifest)
                await _invoke(plugin.configure, None)
                await _invoke(plugin.initialize)
                if record.checkpoint is not None:
                    if not record.descriptor.supports_checkpoint:
                        raise CheckpointUnavailableError(
                            f"checkpoint unsupported: {implementation_id}"
                        )
                    await _invoke(plugin.restore_checkpoint, record.checkpoint)
                healthy = await _invoke(plugin.health_check)
                if healthy is False:
                    raise LifecycleError("health check failed during restore")
                self.registry.set_state(
                    implementation_id, "available", health_score=1.0
                )
                await _invoke(plugin.start)
                await self._emit(
                    "capability.available",
                    record.descriptor,
                    {"state": "available", "restored": True},
                )
            except Exception as exc:  # noqa: BLE001 - restore failures must degrade gracefully
                await self._transition(
                    implementation_id,
                    "temporarily_unavailable",
                    reason_code="restore_failed",
                    message=str(exc),
                )

    async def remove(self, implementation_id: str) -> CapabilityState:
        record = self.registry.record(implementation_id)
        plugin = self._plugins.get(implementation_id)
        checkpoint_error: Exception | None = None
        if plugin is not None:
            await _invoke(plugin.drain)
            try:
                await self.persist_checkpoint(implementation_id)
            except Exception as exc:  # noqa: BLE001 - checkpoint failure must not corrupt removal
                checkpoint_error = exc
            await _invoke(plugin.stop)
            await _invoke(plugin.uninstall)
        affected = self.registry.invalidate_for(implementation_id)
        message = str(checkpoint_error) if checkpoint_error else None
        state = self.registry.set_state(
            implementation_id, "removed", reason_code="removed", message=message
        )
        self._plugins.pop(implementation_id, None)
        await self._emit(
            "capability.removed",
            record.descriptor,
            {
                "state": state.state,
                "blocked_plan_ids": list(affected),
                "checkpoint_error": message,
            },
        )
        return state

    async def persist_checkpoint(self, implementation_id: str) -> None:
        record = self.registry.record(implementation_id)
        plugin = self._plugins.get(implementation_id)
        if plugin is None or not record.descriptor.supports_checkpoint:
            raise CheckpointUnavailableError(
                f"checkpoint unavailable: {implementation_id}"
            )
        checkpoint = await _invoke(plugin.checkpoint)
        if checkpoint is None:
            raise CheckpointUnavailableError(
                f"plugin returned no checkpoint: {implementation_id}"
            )
        self.registry.set_checkpoint(implementation_id, checkpoint)

    async def resolve(
        self, operation: str, preferred_implementation_id: str | None = None
    ) -> Resolution:
        candidates = [
            record
            for record in self.registry.records()
            if self.registry.allows_in_profile(record.descriptor.implementation_id)
            and record.state.state in {"available", "degraded"}
            and record.descriptor.supports(operation)
        ]
        if not candidates:
            raise NoCapabilityProviderError(f"no provider for operation {operation!r}")
        preferred = next(
            (
                item
                for item in candidates
                if item.descriptor.implementation_id == preferred_implementation_id
            ),
            None,
        )
        selected = preferred or max(candidates, key=_resolution_key)
        fallback = (
            preferred_implementation_id is not None
            and selected.descriptor.implementation_id != preferred_implementation_id
        )
        reason = (
            "preferred_provider"
            if preferred is not None
            else ("fallback_provider" if fallback else "highest_priority")
        )
        result = Resolution(
            operation, selected.descriptor.implementation_id, fallback, reason
        )
        await self._emit(
            "capability.resolved",
            selected.descriptor,
            {
                "operation": operation,
                "implementation_id": result.implementation_id,
                "fallback": fallback,
                "reason": reason,
            },
        )
        return result

    def register_plan(
        self, plan_id: str, required_implementation_ids: Iterable[str]
    ) -> None:
        self.registry.register_plan(plan_id, required_implementation_ids)

    def define_profile(
        self, profile_id: str, implementation_ids: Iterable[str]
    ) -> None:
        self.registry.define_profile(profile_id, implementation_ids)

    def activate_profile(self, profile_id: str | None) -> None:
        self.registry.activate_profile(profile_id)

    def _check_dependencies(self, descriptor: CapabilityDescriptor) -> None:
        for requirement in descriptor.mandatory:
            if not any(
                record.state.state in {"available", "degraded"}
                and record.descriptor.supports(requirement.operation)
                and _version_satisfies(
                    record.descriptor.implementation_version, requirement.version_range
                )
                for record in self.registry.records()
                if record.descriptor.implementation_id != descriptor.implementation_id
            ):
                raise LifecycleError(
                    f"missing mandatory dependency: {requirement.operation}"
                )

    async def _transition(
        self,
        implementation_id: str,
        state: str,
        *,
        reason_code: str | None = None,
        message: str | None = None,
        health_score: float | None = None,
    ) -> CapabilityState:
        result = self.registry.set_state(
            implementation_id,
            state,
            reason_code=reason_code,
            message=message,
            health_score=health_score,
        )
        descriptor = self.registry.record(implementation_id).descriptor
        await self._emit(
            f"capability.{state}",
            descriptor,
            {"state": state, "reason_code": reason_code, "message": message},
        )
        return result

    async def _emit(
        self,
        event_type: str,
        descriptor: CapabilityDescriptor,
        payload: Mapping[str, Any],
    ) -> None:
        now = _now()
        event = {
            "schema_version": "1.0",
            "event_id": str(uuid.uuid4()),
            "source": "capability_runtime",
            "event_type": event_type,
            "occurred_at": now,
            "monotonic_ns": time.monotonic_ns(),
            "received_at": now,
            "session_id": self._session_id,
            "actor_id": None,
            "context": {
                "process_name": "eu-digital",
                "window_title": None,
                "document_uri": None,
            },
            "payload": {
                "capability_id": descriptor.capability_id,
                "implementation_id": descriptor.implementation_id,
                **dict(payload),
            },
            "quality": {"confidence": 1.0, "completeness": 1.0, "latency_ms": 0},
            "provenance": {"sensor_id": "capability-runtime", "raw_event_id": None},
            "privacy_class": "internal",
            "tags": ["capability", "lifecycle"],
        }
        await self.event_bus.publish(event)


class ModuleLifecycleManager(CapabilityRuntime):
    """Canonical SPEC-023 name for the runtime lifecycle coordinator."""


async def _invoke(function: Callable[..., Any], *args: Any) -> Any:
    result = function(*args)
    if inspect.isawaitable(result):
        return await result
    return result


def _resolution_key(record: CapabilityRecord) -> tuple[int, int, int, int]:
    states = {"available": 2, "degraded": 1}
    latency = {"realtime": 3, "interactive": 2, "batch": 1}
    cpu = {"low": 3, "medium": 2, "high": 1, "unknown": 0}
    return (
        record.priority,
        states[record.state.state],
        latency[record.descriptor.latency_class],
        cpu[record.descriptor.estimated_cpu_class],
    )


def _state(descriptor: CapabilityDescriptor, state: str) -> CapabilityState:
    return CapabilityState(
        descriptor.capability_id, descriptor.implementation_id, state, _now()
    )


def _version_satisfies(version: str, version_range: str | None) -> bool:
    if version_range is None or version_range.strip() in {"", "*"}:
        return True
    try:
        actual = _version_tuple(version)
        requirement = version_range.strip()
        if requirement.startswith(">="):
            return actual >= _version_tuple(requirement[2:])
        if requirement.startswith(">"):
            return actual > _version_tuple(requirement[1:])
        if requirement.startswith("<="):
            return actual <= _version_tuple(requirement[2:])
        if requirement.startswith("<"):
            return actual < _version_tuple(requirement[1:])
        if requirement.startswith("^"):
            lower = _version_tuple(requirement[1:])
            return actual >= lower and actual[0] == lower[0]
        return actual == _version_tuple(requirement)
    except ValueError:
        return False


def _version_tuple(version: str) -> tuple[int, int, int]:
    parts = version.strip().split(".")
    if len(parts) > 3 or any(not part.isdigit() for part in parts):
        raise ValueError(f"invalid semantic version: {version}")
    numbers = [int(part) for part in parts]
    return tuple((numbers + [0, 0, 0])[:3])  # type: ignore[return-value]


def _now() -> str:
    return datetime.now(UTC).isoformat().replace("+00:00", "Z")
