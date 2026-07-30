#include "core/event_bus.hpp"

#include <cassert>
#include <chrono>
#include <thread>
#include <vector>

using eu_digital::CanonicalEvent;
using eu_digital::EventBus;

CanonicalEvent event(int id, const char* source = "system", const char* type = "fixture") {
    return {.event_id = "event-" + std::to_string(id), .source = source, .event_type = type,
            .payload = "{}", .monotonic_ns = static_cast<std::size_t>(id)};
}

int main() {
    EventBus bus(1);
    std::vector<int> seen;
    bus.subscribe("fixture", "system", [&](const CanonicalEvent& value) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        seen.push_back(static_cast<int>(value.monotonic_ns));
    });
    assert(bus.publish(event(1)) == eu_digital::PublishResult::accepted);
    assert(bus.publish(event(1)) == eu_digital::PublishResult::duplicate);
    assert(bus.publish(event(2)) == eu_digital::PublishResult::accepted);
    bus.replay({event(3)});
    assert((seen == std::vector<int>{1, 2, 3}));

    try {
        bus.publish(CanonicalEvent{});
        assert(false);
    } catch (const std::invalid_argument&) {
        assert(bus.dead_letters().size() == 1);
    }
}
