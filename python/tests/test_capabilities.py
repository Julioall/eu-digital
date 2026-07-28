from __future__ import annotations

import json
import sys
import tempfile
import types
import unittest
from pathlib import Path

from eu_digital_lab.capabilities import (
    CapabilityDescriptor,
    CapabilityPlugin,
    CapabilityRuntime,
    ManifestValidationError,
    ModuleLifecycleManager,
    NoCapabilityProviderError,
    PluginDiscovery,
)
from eu_digital_lab.event_bus import AsyncEventBus
from eu_digital_lab.fixture_reader import read_canonical_event

ROOT = Path(__file__).resolve().parents[2]


def descriptor(
    implementation_id: str, operation: str = "observe.test"
) -> dict[str, object]:
    return {
        "schema_version": "1.0",
        "capability_id": "test.sensor",
        "implementation_id": implementation_id,
        "implementation_version": "1.0.0",
        "kind": "sensor",
        "provides": [
            {
                "operation": operation,
                "input_schema": None,
                "output_schema": "urn:test:observation",
                "modalities": ["test"],
                "side_effect": "none",
                "streaming": False,
            }
        ],
        "requires": {"mandatory": [], "optional": []},
        "runtime": {
            "execution": "in_process",
            "startup": "lazy",
            "supports_hot_plug": True,
            "supports_checkpoint": True,
            "estimated_ram_mb": 1,
            "estimated_cpu_class": "low",
        },
        "quality": {
            "confidence_model": "deterministic-test",
            "latency_class": "realtime",
            "calibration_required": False,
        },
        "permissions": [],
        "config_schema": None,
        "health_check_operation": "health.test",
    }


class FakePlugin(CapabilityPlugin):
    def __init__(self, value: dict[str, object], *, fail_health: bool = False) -> None:
        self.descriptor = CapabilityDescriptor.from_mapping(value)
        self.fail_health = fail_health
        self.calls: list[str] = []
        self.checkpoint_value: dict[str, object] = {"counter": 7}

    def configure(self, _config: dict[str, object] | None = None) -> None:
        self.calls.append("configure")

    def initialize(self) -> None:
        self.calls.append("initialize")

    def calibrate(self) -> None:
        self.calls.append("calibrate")

    def health_check(self) -> bool:
        self.calls.append("health_check")
        return not self.fail_health

    def start(self) -> None:
        self.calls.append("start")

    def drain(self) -> None:
        self.calls.append("drain")

    def checkpoint(self) -> dict[str, object]:
        self.calls.append("checkpoint")
        return dict(self.checkpoint_value)

    def restore_checkpoint(self, value: dict[str, object]) -> None:
        self.calls.append("restore_checkpoint")
        self.checkpoint_value = dict(value)

    def stop(self) -> None:
        self.calls.append("stop")

    def uninstall(self) -> None:
        self.calls.append("uninstall")


class CapabilityTests(unittest.IsolatedAsyncioTestCase):
    async def test_runtime_starts_with_no_optional_capabilities(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            runtime = CapabilityRuntime(
                AsyncEventBus(), Path(directory) / "registry.json"
            )
            self.assertEqual(runtime.registry.records(), ())
            self.assertEqual(
                runtime.self_model.snapshot["capabilities"]["available"], []
            )

    async def test_canonical_lifecycle_manager_name_is_available(self) -> None:
        manager = ModuleLifecycleManager(AsyncEventBus())
        self.assertIsInstance(manager, CapabilityRuntime)

    async def test_manifest_discovery_and_install_without_core_change(self) -> None:
        plugin = FakePlugin(descriptor("fake-a"))
        module = types.ModuleType("fake_capability_module")
        module.build = lambda: plugin
        sys.modules[module.__name__] = module
        try:
            with tempfile.TemporaryDirectory() as directory:
                manifest_path = Path(directory) / "fake.json"
                manifest_path.write_text(
                    json.dumps(
                        {
                            "descriptor": descriptor("fake-a"),
                            "entry_point": "fake_capability_module:build",
                            "priority": 5,
                        }
                    ),
                    encoding="utf-8",
                )
                discovered = PluginDiscovery().discover(Path(directory))
                runtime = CapabilityRuntime(
                    AsyncEventBus(), Path(directory) / "registry.json"
                )
                await runtime.install(discovered[0])
                self.assertEqual(runtime.registry.state("fake-a").state, "available")
                self.assertIn(
                    "fake-a", runtime.self_model.snapshot["capabilities"]["available"]
                )
        finally:
            sys.modules.pop(module.__name__, None)

    async def test_entry_point_discovery_is_supported(self) -> None:
        plugin = FakePlugin(descriptor("entry-point"))

        class EntryPoint:
            name = "entry-point"
            value = "unused:factory"

            def load(self) -> object:
                return lambda: plugin

        discovery = PluginDiscovery(entry_points_provider=lambda _group: [EntryPoint()])
        discovered = discovery.discover()
        self.assertEqual(discovered[0].descriptor.implementation_id, "entry-point")
        runtime = CapabilityRuntime(AsyncEventBus())
        await runtime.install(discovered[0])
        self.assertEqual(runtime.registry.state("entry-point").state, "available")

    async def test_invalid_manifest_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "invalid.json"
            invalid = descriptor("invalid")
            del invalid["provides"]
            path.write_text(
                json.dumps({"descriptor": invalid, "entry_point": "x:y"}),
                encoding="utf-8",
            )
            with self.assertRaises(ManifestValidationError):
                PluginDiscovery().discover(Path(directory))

    async def test_lifecycle_failure_is_isolated(self) -> None:
        bus = AsyncEventBus()
        runtime = CapabilityRuntime(bus)
        plugin = FakePlugin(descriptor("broken"), fail_health=True)
        await runtime.install(runtime.discovery_from_plugin(plugin))
        self.assertEqual(runtime.registry.state("broken").state, "failed")
        self.assertIsNotNone(runtime.registry.state("broken").reason_code)
        self.assertFalse(bus.history == ())

    async def test_mandatory_dependency_is_resolved_before_start(self) -> None:
        runtime = CapabilityRuntime(AsyncEventBus())
        dependent_descriptor = descriptor("dependent", operation="dependent.op")
        dependent_descriptor["requires"] = {
            "mandatory": [{"operation": "dependency.op", "version_range": ">=1.0.0"}],
            "optional": [],
        }
        dependent = FakePlugin(dependent_descriptor)
        failed = await runtime.install(runtime.discovery_from_plugin(dependent))
        self.assertEqual(failed.state, "failed")
        self.assertEqual(failed.reason_code, "missing_dependency")

    async def test_incompatible_dependency_version_is_rejected(self) -> None:
        runtime = CapabilityRuntime(AsyncEventBus())
        provider_descriptor = descriptor("old-provider", operation="dependency.op")
        provider_descriptor["implementation_version"] = "0.5.0"
        await runtime.install(
            runtime.discovery_from_plugin(FakePlugin(provider_descriptor))
        )
        dependent_descriptor = descriptor(
            "version-dependent", operation="dependent.version"
        )
        dependent_descriptor["requires"] = {
            "mandatory": [{"operation": "dependency.op", "version_range": ">=1.0.0"}],
            "optional": [],
        }
        failed = await runtime.install(
            runtime.discovery_from_plugin(FakePlugin(dependent_descriptor))
        )
        self.assertEqual(failed.state, "failed")
        self.assertEqual(failed.reason_code, "missing_dependency")

    async def test_profile_restricts_resolver_to_declared_capabilities(self) -> None:
        runtime = CapabilityRuntime(AsyncEventBus())
        first = FakePlugin(descriptor("profile-a", operation="profile.a"))
        second = FakePlugin(descriptor("profile-b", operation="profile.b"))
        await runtime.install(runtime.discovery_from_plugin(first))
        await runtime.install(runtime.discovery_from_plugin(second))
        runtime.define_profile("only-a", {"profile-a"})
        runtime.activate_profile("only-a")
        self.assertEqual(
            (await runtime.resolve("profile.a")).implementation_id, "profile-a"
        )
        with self.assertRaises(NoCapabilityProviderError):
            await runtime.resolve("profile.b")

    async def test_removed_plugin_can_be_reinstalled_after_validation(self) -> None:
        runtime = CapabilityRuntime(AsyncEventBus())
        first = FakePlugin(descriptor("reinstallable"))
        await runtime.install(runtime.discovery_from_plugin(first))
        await runtime.remove("reinstallable")
        replacement = FakePlugin(descriptor("reinstallable"))
        await runtime.install(runtime.discovery_from_plugin(replacement))
        self.assertEqual(runtime.registry.state("reinstallable").state, "available")
        self.assertIn(
            "reinstallable", runtime.self_model.snapshot["capabilities"]["available"]
        )

    async def test_resolver_uses_priority_and_audits_fallback(self) -> None:
        bus = AsyncEventBus()
        runtime = CapabilityRuntime(bus)
        first = FakePlugin(descriptor("slow"))
        second = FakePlugin(descriptor("fast"))
        await runtime.install(runtime.discovery_from_plugin(first, priority=1))
        await runtime.install(runtime.discovery_from_plugin(second, priority=10))

        result = await runtime.resolve(
            "observe.test", preferred_implementation_id="slow"
        )
        self.assertEqual(result.implementation_id, "slow")
        await runtime.remove("slow")
        fallback = await runtime.resolve(
            "observe.test", preferred_implementation_id="slow"
        )
        self.assertEqual(fallback.implementation_id, "fast")
        self.assertTrue(fallback.fallback)
        self.assertTrue(
            any(item["event_type"] == "capability.resolved" for item in bus.history)
        )

    async def test_no_provider_is_typed_and_does_not_invent_observation(self) -> None:
        runtime = CapabilityRuntime(AsyncEventBus())
        with self.assertRaises(NoCapabilityProviderError):
            await runtime.resolve("missing.operation")

    async def test_removal_preserves_event_history_and_invalidates_plans(self) -> None:
        bus = AsyncEventBus()
        runtime = CapabilityRuntime(bus)
        plugin = FakePlugin(descriptor("removable"))
        await runtime.install(runtime.discovery_from_plugin(plugin))
        historical_event = read_canonical_event(
            ROOT / "contracts" / "fixtures" / "canonical_event.json"
        )
        historical_event["event_id"] = "00000000-0000-4000-8000-000000000099"
        await bus.publish(historical_event)
        runtime.register_plan("plan-1", {"removable"})
        before = len(bus.history)
        await runtime.remove("removable")

        self.assertEqual(runtime.registry.state("removable").state, "removed")
        self.assertEqual(runtime.registry.blocked_plans, ("plan-1",))
        self.assertGreater(len(bus.history), before)
        self.assertIn(
            historical_event["event_id"], {item["event_id"] for item in bus.history}
        )
        self.assertEqual(plugin.calls[-3:], ["checkpoint", "stop", "uninstall"])
        self.assertLess(plugin.calls.index("drain"), plugin.calls.index("stop"))
        self.assertIn(
            "removable", runtime.self_model.snapshot["capabilities"]["removed"]
        )

    async def test_restart_restores_state_and_checkpoint(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "registry.json"
            first_runtime = CapabilityRuntime(AsyncEventBus(), path)
            first = FakePlugin(descriptor("persistent"))
            await first_runtime.install(first_runtime.discovery_from_plugin(first))
            await first_runtime.persist_checkpoint("persistent")

            second_runtime = CapabilityRuntime(AsyncEventBus(), path)
            second = FakePlugin(descriptor("persistent"))
            await second_runtime.restore([second_runtime.discovery_from_plugin(second)])
            self.assertEqual(
                second_runtime.registry.state("persistent").state, "available"
            )
            self.assertEqual(second.checkpoint_value, {"counter": 7})
            self.assertIn("restore_checkpoint", second.calls)

    async def test_core_has_no_concrete_capability_imports(self) -> None:
        source = (ROOT / "python" / "eu_digital_lab" / "capabilities.py").read_text(
            encoding="utf-8"
        )
        self.assertNotRegex(source, r"(?:sensors|tools|actions)\.")
        for path in (ROOT / "cpp" / "core").glob("*.hpp"):
            self.assertNotRegex(
                path.read_text(encoding="utf-8"), r"(?:sensors|tools|actions)\."
            )


class CapabilitySchemaTests(unittest.TestCase):
    def test_descriptor_and_contracts_are_schema_backed(self) -> None:
        valid = CapabilityDescriptor.from_mapping(descriptor("valid"))
        self.assertEqual(valid.implementation_id, "valid")
        invalid = descriptor("invalid")
        invalid["runtime"] = {"execution": "remote"}
        with self.assertRaises(ManifestValidationError):
            CapabilityDescriptor.from_mapping(invalid)

    def test_all_spec_023_shared_schemas_are_valid_json(self) -> None:
        for name in (
            "capability_descriptor.schema.json",
            "capability_state.schema.json",
            "observation_envelope.schema.json",
            "self_model.schema.json",
            "plugin_manifest.schema.json",
        ):
            value = json.loads(
                (ROOT / "contracts" / "schemas" / name).read_text(encoding="utf-8")
            )
            self.assertEqual(
                value["$schema"], "https://json-schema.org/draft/2020-12/schema"
            )


if __name__ == "__main__":
    unittest.main()
