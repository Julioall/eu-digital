"""In-process asynchronous CanonicalEvent bus for the laboratory runtime."""

from __future__ import annotations

import asyncio
from dataclasses import dataclass
from typing import Any, Mapping

from .fixture_reader import validate_canonical_event


class InvalidCanonicalEvent(ValueError):
    """Raised when an event does not satisfy the shared CanonicalEvent schema."""


@dataclass(frozen=True)
class DeadLetter:
    sequence: int
    event_id: str | None
    reason: str
    payload: Any


@dataclass(frozen=True)
class PublishReceipt:
    accepted: bool
    duplicate: bool


class Subscription:
    def __init__(self, event_type: str | None, source: str | None, max_queue_size: int) -> None:
        self.event_type = event_type
        self.source = source
        self._queue: asyncio.Queue[dict[str, Any]] = asyncio.Queue(maxsize=max_queue_size)
        self.closed = False

    def matches(self, event: Mapping[str, Any]) -> bool:
        return (self.event_type is None or event["event_type"] == self.event_type) and (
            self.source is None or event["source"] == self.source
        )

    async def get(self) -> dict[str, Any]:
        return await self._queue.get()

    def close(self) -> None:
        self.closed = True


class AsyncEventBus:
    """Bounded, ordered, idempotent in-process event delivery."""

    def __init__(self, max_queue_size: int = 128) -> None:
        if max_queue_size <= 0:
            raise ValueError("max_queue_size must be positive")
        self.max_queue_size = max_queue_size
        self._subscriptions: list[Subscription] = []
        self._processed_event_ids: set[str] = set()
        self._history: list[dict[str, Any]] = []
        self._dead_letters: list[DeadLetter] = []
        self._sequence = 0
        self._lock = asyncio.Lock()

    @property
    def history(self) -> tuple[dict[str, Any], ...]:
        return tuple(self._history)

    @property
    def processed_event_ids(self) -> frozenset[str]:
        return frozenset(self._processed_event_ids)

    @property
    def dead_letters(self) -> tuple[DeadLetter, ...]:
        return tuple(self._dead_letters)

    def subscribe(
        self,
        *,
        event_type: str | None = None,
        source: str | None = None,
        max_queue_size: int | None = None,
    ) -> Subscription:
        size = self.max_queue_size if max_queue_size is None else max_queue_size
        if size <= 0:
            raise ValueError("subscription max_queue_size must be positive")
        subscription = Subscription(event_type, source, size)
        self._subscriptions.append(subscription)
        return subscription

    def unsubscribe(self, subscription: Subscription) -> None:
        subscription.close()
        if subscription in self._subscriptions:
            self._subscriptions.remove(subscription)

    async def publish(self, event: Mapping[str, Any]) -> PublishReceipt:
        try:
            validate_canonical_event(event)
        except (ValueError, TypeError) as exc:
            self._sequence += 1
            event_id = event.get("event_id") if isinstance(event, Mapping) else None
            self._dead_letters.append(DeadLetter(self._sequence, event_id, str(exc), event))
            raise InvalidCanonicalEvent(str(exc)) from exc

        immutable = dict(event)
        async with self._lock:
            event_id = immutable["event_id"]
            if event_id in self._processed_event_ids:
                return PublishReceipt(accepted=False, duplicate=True)
            self._sequence += 1
            for subscription in tuple(self._subscriptions):
                if not subscription.closed and subscription.matches(immutable):
                    await subscription._queue.put(immutable.copy())
            self._processed_event_ids.add(event_id)
            self._history.append(immutable.copy())
            return PublishReceipt(accepted=True, duplicate=False)

    async def replay(self, events: list[Mapping[str, Any]] | tuple[Mapping[str, Any], ...]) -> None:
        for event in events:
            await self.publish(event)
