import ast
import asyncio
import sys
import unittest
from pathlib import Path
from typing import Any

LAB_ROOT = Path(__file__).resolve().parents[1]
if str(LAB_ROOT) not in sys.path:
    sys.path.insert(0, str(LAB_ROOT))

from eu_digital_lab.local_model_gateway import (
    ABLATION,
    BASELINE_SCHEDULER_ID,
    FALSIFICATION,
    HYPOTHESIS,
    PRIORITY_SCHEDULER_ID,
    GatewayConfig,
    InvalidModelResponseError,
    LocalModelGateway,
    ModelGatewayCancelledError,
    ModelGatewayError,
    ModelGatewayTimeoutError,
    ModelRequest,
    PromptTemplate,
    SchedulingPolicy,
)


class FakeBackend:
    def __init__(
        self,
        backend_id: str,
        *,
        delay_seconds: float = 0.0,
        output: Any | None = None,
    ) -> None:
        self.backend_id = backend_id
        self.delay_seconds = delay_seconds
        self.output = output if output is not None else {"kind": "summary", "fields": {}}
        self.events: list[tuple[str, str]] = []
        self.in_flight = 0
        self.max_in_flight = 0
        self.cancelled: list[str] = []
        self.release = asyncio.Event()
        self.release.set()

    async def load(self, model_id: str) -> None:
        self.events.append(("load", model_id))

    async def invoke(self, request: ModelRequest) -> Any:
        self.events.append(("invoke", request.request_id))
        self.in_flight += 1
        self.max_in_flight = max(self.max_in_flight, self.in_flight)
        try:
            if self.delay_seconds:
                await asyncio.sleep(self.delay_seconds)
            await self.release.wait()
            return self.output
        finally:
            self.in_flight -= 1

    async def cancel(self, request_id: str) -> None:
        self.cancelled.append(request_id)
        self.events.append(("cancel", request_id))
        self.release.set()

    async def unload(self, model_id: str) -> None:
        self.events.append(("unload", model_id))


TEMPLATE = PromptTemplate(
    template_id="summarize",
    version="1.0.0",
    body="Summarize: {content}",
    variables=("content",),
)


class LocalModelGatewayTests(unittest.IsolatedAsyncioTestCase):
    def gateway(
        self,
        *backends: FakeBackend,
        policy: SchedulingPolicy = SchedulingPolicy.priority_single_worker_v1,
    ) -> LocalModelGateway:
        return LocalModelGateway(
            {backend.backend_id: backend for backend in backends},
            GatewayConfig(backend_id=backends[0].backend_id, scheduling_policy=policy),
        )

    def request(
        self,
        request_id: str,
        *,
        backend_id: str = "a",
        priority: int = 0,
        timeout_seconds: float = 1.0,
    ) -> ModelRequest:
        return ModelRequest(
            request_id=request_id,
            backend_id=backend_id,
            model_id="fixture-model",
            priority=priority,
            timeout_seconds=timeout_seconds,
            template=TEMPLATE,
            rendered_prompt=TEMPLATE.render({"content": request_id}),
        )

    async def test_never_runs_two_heavy_inferences_at_once(self) -> None:
        backend = FakeBackend("a", delay_seconds=0.01)
        gateway = self.gateway(backend)

        first = gateway.submit(self.request("first"))
        second = gateway.submit(self.request("second"))
        await asyncio.gather(first, second)

        self.assertEqual(backend.max_in_flight, 1)
        self.assertEqual(gateway.metrics()["max_concurrent_inferences"], 1)
        self.assertEqual(gateway.metrics()["max_loaded_models"], 1)

    async def test_priority_and_fifo_are_selectable_with_same_interface(self) -> None:
        priority_backend = FakeBackend("a")
        priority_gateway = self.gateway(priority_backend)
        low = priority_gateway.submit(self.request("low", priority=1))
        high = priority_gateway.submit(self.request("high", priority=9))
        await asyncio.gather(low, high)

        fifo_backend = FakeBackend("a")
        fifo_gateway = self.gateway(
            fifo_backend, policy=SchedulingPolicy.fifo_single_worker_v0
        )
        first = fifo_gateway.submit(self.request("first", priority=1))
        second = fifo_gateway.submit(self.request("second", priority=9))
        await asyncio.gather(first, second)

        self.assertEqual(
            [identifier for event, identifier in priority_backend.events if event == "invoke"],
            ["high", "low"],
        )
        self.assertEqual(
            [identifier for event, identifier in fifo_backend.events if event == "invoke"],
            ["first", "second"],
        )
        self.assertEqual(priority_gateway.metrics()["scheduler_id"], PRIORITY_SCHEDULER_ID)
        self.assertEqual(fifo_gateway.metrics()["scheduler_id"], BASELINE_SCHEDULER_ID)

    async def test_timeout_cancels_and_unloads_resources(self) -> None:
        backend = FakeBackend("a")
        backend.release.clear()
        gateway = self.gateway(backend)

        with self.assertRaises(ModelGatewayTimeoutError):
            await gateway.invoke(self.request("slow", timeout_seconds=0.01))

        self.assertEqual(backend.cancelled, ["slow"])
        self.assertIn(("unload", "fixture-model"), backend.events)
        self.assertEqual(gateway.metrics()["loaded_model_id"], None)

    async def test_explicit_cancellation_removes_queued_request(self) -> None:
        backend = FakeBackend("a", delay_seconds=0.03)
        gateway = self.gateway(backend)
        active = gateway.submit(self.request("active"))
        queued = gateway.submit(self.request("queued"))
        self.assertTrue(await gateway.cancel("queued"))

        await active
        with self.assertRaises(ModelGatewayCancelledError):
            await queued
        self.assertNotIn(("invoke", "queued"), backend.events)

    async def test_invalid_backend_output_is_rejected(self) -> None:
        gateway = self.gateway(FakeBackend("a", output={"text": "unstructured"}))

        with self.assertRaises(InvalidModelResponseError):
            await gateway.invoke(self.request("invalid"))

    async def test_backend_can_be_swapped_by_configuration(self) -> None:
        first = FakeBackend("a")
        second = FakeBackend("b")
        gateway = self.gateway(first, second)
        await gateway.invoke(self.request("first", backend_id="a"))
        await gateway.configure_backend("b")
        await gateway.invoke(self.request("second", backend_id="b"))

        self.assertIn(("invoke", "first"), first.events)
        self.assertIn(("invoke", "second"), second.events)
        self.assertEqual(gateway.config.backend_id, "b")

    def test_template_is_versioned_and_rejects_missing_or_extra_variables(self) -> None:
        self.assertEqual(TEMPLATE.render({"content": "local"}), "Summarize: local")
        with self.assertRaises(ModelGatewayError):
            TEMPLATE.render({})
        with self.assertRaises(ModelGatewayError):
            TEMPLATE.render({"content": "x", "extra": "y"})

    async def test_same_replay_is_deterministic_and_has_no_llm_or_api_import(self) -> None:
        first_backend = FakeBackend("a")
        second_backend = FakeBackend("a")
        first = self.gateway(first_backend)
        second = self.gateway(second_backend)
        for gateway in (first, second):
            low = gateway.submit(self.request("low", priority=1))
            high = gateway.submit(self.request("high", priority=9))
            await asyncio.gather(low, high)
        source = (LAB_ROOT / "eu_digital_lab" / "local_model_gateway.py").read_text(
            encoding="utf-8"
        )
        tree = ast.parse(source)
        imports = [
            alias.name
            for node in ast.walk(tree)
            if isinstance(node, ast.Import)
            for alias in node.names
        ]
        imports.extend(
            node.module or "" for node in ast.walk(tree) if isinstance(node, ast.ImportFrom)
        )

        self.assertEqual(first.snapshot(), second.snapshot())
        self.assertFalse(
            any(term in name.lower() for name in imports for term in ("llm", "requests", "http"))
        )

    def test_scientific_metadata_is_registered(self) -> None:
        metrics = self.gateway(FakeBackend("a")).metrics()

        self.assertEqual(metrics["hypothesis"], HYPOTHESIS)
        self.assertEqual(metrics["ablation"], ABLATION)
        self.assertEqual(metrics["falsification"], FALSIFICATION)


if __name__ == "__main__":
    unittest.main()
