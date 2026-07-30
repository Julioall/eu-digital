from __future__ import annotations

import asyncio
import copy
import json
import unittest
from pathlib import Path

from eu_digital_lab.event_bus import AsyncEventBus, InvalidCanonicalEvent
from eu_digital_lab.fixture_reader import read_canonical_event


ROOT = Path(__file__).resolve().parents[2]
FIXTURE = ROOT / "contracts" / "fixtures" / "canonical_event.json"


def event(number: int, *, source: str = "system", event_type: str = "fixture.canonical_event") -> dict:
    value = copy.deepcopy(read_canonical_event(FIXTURE))
    value["event_id"] = f"00000000-0000-4000-8000-{number:012d}"
    value["source"] = source
    value["event_type"] = event_type
    value["monotonic_ns"] = number
    return value


class EventBusTests(unittest.IsolatedAsyncioTestCase):
    async def test_filters_and_preserves_order_per_source(self) -> None:
        bus = AsyncEventBus()
        subscription = bus.subscribe(source="keyboard", event_type="input.key")

        await bus.publish(event(1, source="keyboard", event_type="input.key"))
        await bus.publish(event(2, source="keyboard", event_type="input.key"))
        await bus.publish(event(3, source="mouse", event_type="input.pointer"))

        self.assertEqual((await subscription.get())["monotonic_ns"], 1)
        self.assertEqual((await subscription.get())["monotonic_ns"], 2)
        self.assertEqual(bus.history[0]["monotonic_ns"], 1)

    async def test_duplicate_event_id_is_not_delivered_twice(self) -> None:
        bus = AsyncEventBus()
        subscription = bus.subscribe()
        first = event(1)
        duplicate = copy.deepcopy(first)

        self.assertFalse((await bus.publish(first)).duplicate)
        self.assertTrue((await bus.publish(duplicate)).duplicate)
        self.assertEqual((await subscription.get())["event_id"], first["event_id"])
        self.assertEqual(bus.processed_event_ids, frozenset({first["event_id"]}))

    async def test_slow_consumer_applies_explicit_backpressure(self) -> None:
        bus = AsyncEventBus(max_queue_size=1)
        subscription = bus.subscribe(max_queue_size=1)
        await bus.publish(event(1))

        producer = asyncio.create_task(bus.publish(event(2)))
        await asyncio.sleep(0)
        self.assertFalse(producer.done())

        self.assertEqual((await subscription.get())["monotonic_ns"], 1)
        await asyncio.wait_for(producer, timeout=1)
        self.assertEqual((await subscription.get())["monotonic_ns"], 2)

    async def test_invalid_event_is_typed_error_and_dead_lettered(self) -> None:
        bus = AsyncEventBus()
        invalid = event(1)
        del invalid["event_type"]

        with self.assertRaises(InvalidCanonicalEvent):
            await bus.publish(invalid)

        self.assertEqual(len(bus.dead_letters), 1)
        self.assertEqual(bus.dead_letters[0].event_id, invalid["event_id"])

    async def test_replay_republishes_events_in_order(self) -> None:
        bus = AsyncEventBus()
        subscription = bus.subscribe()
        events = [event(1), event(2)]

        await bus.replay(events)

        self.assertEqual([((await subscription.get())["monotonic_ns"]) for _ in events], [1, 2])


if __name__ == "__main__":
    unittest.main()
