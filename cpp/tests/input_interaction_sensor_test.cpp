#include "core/input_interaction_sensor.hpp"

#include <cassert>
#include <string>
#include <vector>

using eu_digital::CanonicalEvent;
using eu_digital::InputInteractionConfig;
using eu_digital::InputInteractionSensor;
using eu_digital::RawInputEvent;
using eu_digital::RawInputKind;
using eu_digital::WindowContext;

int main() {
    std::vector<CanonicalEvent> events;
    InputInteractionConfig config;
    config.emit_raw_events = true;
    config.capture_key_codes = true;
    config.aggregate_window_ms = 2000;
    config.pause_threshold_ms = 500;
    InputInteractionSensor sensor(
        [&](const CanonicalEvent& event) { events.push_back(event); }, config);

    assert(sensor.descriptor().capability_id == "interaction.input");
    const WindowContext editor{10, "editor.exe", "Editor"};
    sensor.ingest(RawInputEvent::key_down(65, 0, false, false, false), editor);
    sensor.ingest(RawInputEvent::key_down(66, 100, false, false, false), editor);
    sensor.ingest(RawInputEvent::key_down(67, 1000, true, false, false), editor);
    sensor.ingest(RawInputEvent::mouse_move(1100, 3, 4), editor);
    sensor.flush();

    bool saw_raw_key = false;
    bool saw_aggregate = false;
    bool saw_clipboard = false;
    for (const auto& event : events) {
        assert(event.payload.find("payload_schema_version") != std::string::npos);
        assert(event.payload.find("editor.exe") != std::string::npos);
        saw_raw_key = saw_raw_key || event.event_type == "input.key";
        if (event.event_type == "input.aggregate") {
            saw_aggregate = true;
            assert(event.payload.find("key_down_count\":3") != std::string::npos);
            assert(event.payload.find("pause_count\":1") != std::string::npos);
            assert(event.payload.find("shortcut_count\":1") != std::string::npos);
            assert(event.payload.find("mouse_move_count\":1") != std::string::npos);
            assert(event.payload.find("mouse_distance\":7") != std::string::npos);
        }
    }
    assert(saw_raw_key);
    assert(saw_aggregate);

    const std::size_t before_clipboard = events.size();
    sensor.ingest(RawInputEvent::clipboard(1200, 42, "digest"), editor);
    assert(events.size() == before_clipboard + 1);
    assert(events.back().event_type == "input.clipboard");
    assert(events.back().payload.find("content_length\":42") != std::string::npos);
    assert(events.back().payload.find("digest") != std::string::npos);

    InputInteractionConfig aggregate_only;
    aggregate_only.emit_raw_events = false;
    InputInteractionSensor aggregate_sensor([&](const CanonicalEvent& event) { events.push_back(event); }, aggregate_only);
    const std::size_t before_aggregate_only = events.size();
    aggregate_sensor.ingest(RawInputEvent::mouse_move(0, 1, 1), editor);
    assert(events.size() == before_aggregate_only);
    aggregate_sensor.flush();
    assert(events.size() == before_aggregate_only + 1);
    assert(events.back().event_type == "input.aggregate");
}
