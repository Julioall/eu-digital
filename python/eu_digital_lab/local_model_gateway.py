"""Local, backend-agnostic gateway with one heavy inference worker.

The gateway schedules requests and validates structured output. It never
downloads a model, calls a remote API, generates dialogue, or executes tools.
"""

from __future__ import annotations

import asyncio
import heapq
import json
import time
import uuid
from collections.abc import Mapping
from dataclasses import dataclass
from enum import Enum
from string import Formatter
from typing import Any, Protocol

from .schema_validation import validate_shared_schema

SCHEMA_VERSION = "1.0"
PRIORITY_SCHEDULER_ID = "priority_single_worker_v1"
BASELINE_SCHEDULER_ID = "fifo_single_worker_v0"
HYPOTHESIS = (
    "a single-worker local gateway with stable priority ordering preserves the "
    "one-heavy-model resource bound while reducing priority wait versus FIFO"
)
ABLATION = (
    "select fifo_single_worker_v0 through the same gateway interface while "
    "retaining the single-worker resource bound"
)
FALSIFICATION = (
    "two heavy inferences or loaded models coexist, a timeout retains a "
    "resource, or invalid structured output is returned to a caller"
)
_NAMESPACE = uuid.UUID("8272847f-ce45-4630-8ec5-5ca8b5a83c49")


class ModelGatewayError(RuntimeError):
    """Base typed error for local model gateway failures."""


class ModelGatewayTimeoutError(ModelGatewayError):
    """A local backend did not complete before its declared timeout."""


class ModelGatewayCancelledError(ModelGatewayError):
    """A queued or active request was cancelled locally."""


class InvalidModelResponseError(ModelGatewayError):
    """A backend result did not satisfy the structured response contract."""


class SchedulingPolicy(str, Enum):
    priority_single_worker_v1 = PRIORITY_SCHEDULER_ID
    fifo_single_worker_v0 = BASELINE_SCHEDULER_ID


class LocalModelBackend(Protocol):
    """Port implemented by an optional local inference backend."""

    backend_id: str

    async def load(self, model_id: str) -> None: ...

    async def invoke(self, request: ModelRequest) -> Mapping[str, Any]: ...

    async def cancel(self, request_id: str) -> None: ...

    async def unload(self, model_id: str) -> None: ...


@dataclass(frozen=True)
class PromptTemplate:
    """A local versioned template with an exact set of allowed variables."""

    template_id: str
    version: str
    body: str
    variables: tuple[str, ...]

    def __post_init__(self) -> None:
        _required_string(self.template_id, "template_id")
        _required_string(self.version, "template_version")
        _required_string(self.body, "template_body")
        normalized = _strings(self.variables, "template_variables")
        if len(set(normalized)) != len(normalized):
            raise ModelGatewayError("template variables must be unique")
        referenced = tuple(
            field_name
            for _, field_name, _, _ in Formatter().parse(self.body)
            if field_name is not None
        )
        if set(referenced) != set(normalized):
            raise ModelGatewayError("template variables must match template placeholders")
        if any(not field.isidentifier() for field in referenced):
            raise ModelGatewayError("template placeholders must be simple identifiers")
        object.__setattr__(self, "variables", normalized)
        self.to_mapping()

    def render(self, values: Mapping[str, str]) -> str:
        if not isinstance(values, Mapping):
            raise ModelGatewayError("template values must be a mapping")
        normalized = {str(key): value for key, value in values.items()}
        if set(normalized) != set(self.variables):
            raise ModelGatewayError("template values must exactly match declared variables")
        if any(not isinstance(value, str) for value in normalized.values()):
            raise ModelGatewayError("template values must be strings")
        try:
            return self.body.format(**normalized)
        except (KeyError, ValueError) as error:
            raise ModelGatewayError("template rendering failed") from error

    def to_mapping(self) -> dict[str, Any]:
        value = {
            "template_id": self.template_id,
            "version": self.version,
            "body": self.body,
            "variables": list(self.variables),
        }
        _validate_contract(value, "model_prompt_template.schema.json")
        return value


@dataclass(frozen=True)
class ModelRequest:
    """A local request that is safe to queue without a concrete backend import."""

    request_id: str
    backend_id: str
    model_id: str
    priority: int
    timeout_seconds: float
    template: PromptTemplate
    rendered_prompt: str
    schema_version: str = SCHEMA_VERSION

    def __post_init__(self) -> None:
        _required_string(self.request_id, "request_id")
        _required_string(self.backend_id, "backend_id")
        _required_string(self.model_id, "model_id")
        if not isinstance(self.priority, int) or isinstance(self.priority, bool) or self.priority < 0:
            raise ModelGatewayError("priority must be a non-negative integer")
        if (
            isinstance(self.timeout_seconds, bool)
            or not isinstance(self.timeout_seconds, (int, float))
            or self.timeout_seconds <= 0
        ):
            raise ModelGatewayError("timeout_seconds must be positive")
        _required_string(self.rendered_prompt, "rendered_prompt")
        if self.schema_version != SCHEMA_VERSION:
            raise ModelGatewayError("unsupported model request schema version")
        self.to_mapping()

    def to_mapping(self) -> dict[str, Any]:
        value = {
            "request_id": self.request_id,
            "schema_version": self.schema_version,
            "backend_id": self.backend_id,
            "model_id": self.model_id,
            "priority": self.priority,
            "timeout_seconds": float(self.timeout_seconds),
            "template": {
                "template_id": self.template.template_id,
                "version": self.template.version,
            },
            "rendered_prompt": self.rendered_prompt,
        }
        _validate_contract(value, "local_model_request.schema.json")
        return value


@dataclass(frozen=True)
class ModelResponse:
    """Validated structured output from a local backend."""

    response_id: str
    request_id: str
    backend_id: str
    model_id: str
    output_kind: str
    output_fields: dict[str, Any]
    latency_ms: float
    schema_version: str = SCHEMA_VERSION

    def to_mapping(self) -> dict[str, Any]:
        value = {
            "response_id": self.response_id,
            "schema_version": self.schema_version,
            "request_id": self.request_id,
            "backend_id": self.backend_id,
            "model_id": self.model_id,
            "status": "completed",
            "output": {"kind": self.output_kind, "fields": self.output_fields},
            "latency_ms": self.latency_ms,
        }
        _validate_contract(value, "local_model_response.schema.json")
        return value


@dataclass(frozen=True)
class GatewayConfig:
    """Local selection and scheduling configuration for the injected ports."""

    backend_id: str
    scheduling_policy: SchedulingPolicy = SchedulingPolicy.priority_single_worker_v1
    unload_after_request: bool = True

    def __post_init__(self) -> None:
        _required_string(self.backend_id, "backend_id")
        try:
            object.__setattr__(self, "scheduling_policy", SchedulingPolicy(self.scheduling_policy))
        except ValueError as error:
            raise ModelGatewayError("unsupported scheduling policy") from error


@dataclass
class _Job:
    request: ModelRequest
    future: asyncio.Future[ModelResponse]
    sequence: int
    cancelled: bool = False


class LocalModelGateway:
    """Schedule one local heavy backend request at a time through a port."""

    def __init__(
        self, backends: Mapping[str, LocalModelBackend], config: GatewayConfig
    ) -> None:
        self._backends = dict(backends)
        if not self._backends:
            raise ModelGatewayError("at least one local backend is required")
        if config.backend_id not in self._backends:
            raise ModelGatewayError("configured backend is unavailable")
        if any(backend_id != backend.backend_id for backend_id, backend in self._backends.items()):
            raise ModelGatewayError("backend mapping key must match backend_id")
        self.config = config
        self._queue: list[tuple[int, int, _Job]] = []
        self._jobs: dict[str, _Job] = {}
        self._sequence = 0
        self._worker: asyncio.Task[None] | None = None
        self._active: _Job | None = None
        self._loaded: tuple[str, str] | None = None
        self._audit: list[dict[str, str]] = []
        self._max_concurrent_inferences = 0
        self._concurrent_inferences = 0
        self._max_loaded_models = 0
        self._closed = False

    def submit(self, request: ModelRequest) -> asyncio.Future[ModelResponse]:
        """Enqueue a request and return its future without invoking immediately."""

        if self._closed:
            raise ModelGatewayError("gateway is closed")
        if request.backend_id != self.config.backend_id:
            raise ModelGatewayError("request backend does not match configured backend")
        if request.request_id in self._jobs:
            raise ModelGatewayError("request_id is already queued or active")
        loop = asyncio.get_running_loop()
        future: asyncio.Future[ModelResponse] = loop.create_future()
        job = _Job(request=request, future=future, sequence=self._sequence)
        self._sequence += 1
        priority = self._queue_priority(job)
        heapq.heappush(self._queue, (priority, job.sequence, job))
        self._jobs[request.request_id] = job
        self._audit.append({"event": "queued", "request_id": request.request_id})
        if self._worker is None or self._worker.done():
            self._worker = asyncio.create_task(self._run_worker())
        return future

    async def invoke(self, request: ModelRequest) -> ModelResponse:
        """Submit and await one validated local response."""

        return await self.submit(request)

    async def cancel(self, request_id: str) -> bool:
        """Cancel a queued request or forward cancellation to the active backend."""

        job = self._jobs.get(request_id)
        if job is None or job.future.done():
            return False
        job.cancelled = True
        self._audit.append({"event": "cancel_requested", "request_id": request_id})
        if self._active is job:
            await self._backends[job.request.backend_id].cancel(request_id)
            return True
        job.future.set_exception(ModelGatewayCancelledError("request cancelled before inference"))
        return True

    async def configure_backend(self, backend_id: str) -> None:
        """Swap the selected local backend only while the worker is idle."""

        _required_string(backend_id, "backend_id")
        if backend_id not in self._backends:
            raise ModelGatewayError("configured backend is unavailable")
        if self._active is not None or self._queue:
            raise ModelGatewayError("cannot change backend while work is pending")
        await self._unload_current()
        self.config = GatewayConfig(
            backend_id=backend_id,
            scheduling_policy=self.config.scheduling_policy,
            unload_after_request=self.config.unload_after_request,
        )
        self._audit.append({"event": "backend_configured", "request_id": backend_id})

    async def close(self) -> None:
        """Release the active local model after pending work has completed."""

        if self._worker is not None:
            await self._worker
        await self._unload_current()
        self._closed = True

    def metrics(self) -> dict[str, Any]:
        """Operational measurements, not a claim about model intelligence."""

        return {
            "scheduler_id": self.config.scheduling_policy.value,
            "baseline_scheduler_id": BASELINE_SCHEDULER_ID,
            "hypothesis": HYPOTHESIS,
            "ablation": ABLATION,
            "falsification": FALSIFICATION,
            "max_concurrent_inferences": self._max_concurrent_inferences,
            "max_loaded_models": self._max_loaded_models,
            "loaded_model_id": None if self._loaded is None else self._loaded[1],
            "queued_request_count": sum(not job.cancelled for _, _, job in self._queue),
            "active_request_id": None if self._active is None else self._active.request.request_id,
        }

    def snapshot(self) -> dict[str, Any]:
        """Return deterministic scheduling/audit state without prompt contents."""

        return {
            "config": {
                "backend_id": self.config.backend_id,
                "scheduling_policy": self.config.scheduling_policy.value,
                "unload_after_request": self.config.unload_after_request,
            },
            "audit": list(self._audit),
            "metrics": self.metrics(),
        }

    async def _run_worker(self) -> None:
        await asyncio.sleep(0)
        while self._queue:
            _, _, job = heapq.heappop(self._queue)
            if job.cancelled:
                self._jobs.pop(job.request.request_id, None)
                continue
            self._active = job
            try:
                response = await self._execute(job)
                if job.cancelled:
                    raise ModelGatewayCancelledError("request cancelled during inference")
                if not job.future.done():
                    job.future.set_result(response)
                self._audit.append({"event": "completed", "request_id": job.request.request_id})
            except ModelGatewayError as error:
                if not job.future.done():
                    job.future.set_exception(error)
                self._audit.append({"event": type(error).__name__, "request_id": job.request.request_id})
            except Exception as error:  # noqa: BLE001 - isolate arbitrary backend failures
                wrapped = ModelGatewayError(f"local backend failed: {error}")
                if not job.future.done():
                    job.future.set_exception(wrapped)
                self._audit.append({"event": type(wrapped).__name__, "request_id": job.request.request_id})
            finally:
                self._jobs.pop(job.request.request_id, None)
                self._active = None
        self._worker = None

    async def _execute(self, job: _Job) -> ModelResponse:
        request = job.request
        backend = self._backends[request.backend_id]
        await self._load(request.backend_id, request.model_id)
        started = time.monotonic()
        self._concurrent_inferences += 1
        self._max_concurrent_inferences = max(
            self._max_concurrent_inferences, self._concurrent_inferences
        )
        try:
            try:
                raw_output = await asyncio.wait_for(
                    backend.invoke(request), timeout=float(request.timeout_seconds)
                )
            except TimeoutError as error:
                await backend.cancel(request.request_id)
                raise ModelGatewayTimeoutError("local model request timed out") from error
            if job.cancelled:
                raise ModelGatewayCancelledError("request cancelled during inference")
            response = _structured_response(request, raw_output, (time.monotonic() - started) * 1000)
            response.to_mapping()
            return response
        finally:
            self._concurrent_inferences -= 1
            if self.config.unload_after_request:
                await self._unload_current()

    async def _load(self, backend_id: str, model_id: str) -> None:
        if self._loaded == (backend_id, model_id):
            return
        await self._unload_current()
        await self._backends[backend_id].load(model_id)
        self._loaded = (backend_id, model_id)
        self._max_loaded_models = max(self._max_loaded_models, 1)
        self._audit.append({"event": "loaded", "request_id": model_id})

    async def _unload_current(self) -> None:
        if self._loaded is None:
            return
        backend_id, model_id = self._loaded
        try:
            await self._backends[backend_id].unload(model_id)
        finally:
            self._audit.append({"event": "unloaded", "request_id": model_id})
            self._loaded = None

    def _queue_priority(self, job: _Job) -> int:
        if self.config.scheduling_policy is SchedulingPolicy.fifo_single_worker_v0:
            return 0
        return -job.request.priority


def _structured_response(
    request: ModelRequest, raw_output: Mapping[str, Any], latency_ms: float
) -> ModelResponse:
    if not isinstance(raw_output, Mapping) or set(raw_output) != {"kind", "fields"}:
        raise InvalidModelResponseError("backend output must contain only kind and fields")
    kind = raw_output["kind"]
    fields = raw_output["fields"]
    if not isinstance(kind, str) or not kind.strip() or not isinstance(fields, Mapping):
        raise InvalidModelResponseError("backend output has invalid structured fields")
    try:
        normalized_fields = json.loads(json.dumps(dict(fields), sort_keys=True))
    except (TypeError, ValueError) as error:
        raise InvalidModelResponseError("backend output fields must be JSON-compatible") from error
    return ModelResponse(
        response_id=str(
            uuid.uuid5(
                _NAMESPACE,
                f"{request.request_id}:{request.backend_id}:{request.model_id}:{kind}",
            )
        ),
        request_id=request.request_id,
        backend_id=request.backend_id,
        model_id=request.model_id,
        output_kind=kind,
        output_fields=normalized_fields,
        latency_ms=max(0.0, latency_ms),
    )


def _validate_contract(value: Mapping[str, Any], schema_name: str) -> None:
    try:
        validate_shared_schema(value, schema_name)
    except ValueError as error:
        raise ModelGatewayError(str(error)) from error


def _required_string(value: str, name: str) -> None:
    if not isinstance(value, str) or not value.strip():
        raise ModelGatewayError(f"{name} must be a non-empty string")


def _strings(values: tuple[str, ...], name: str) -> tuple[str, ...]:
    if isinstance(values, str) or not isinstance(values, tuple):
        raise ModelGatewayError(f"{name} must be a tuple of strings")
    if any(not isinstance(value, str) or not value.strip() for value in values):
        raise ModelGatewayError(f"{name} must contain non-empty strings")
    return values
