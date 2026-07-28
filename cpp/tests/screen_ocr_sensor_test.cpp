#include "core/screen_ocr_sensor.hpp"

#include <algorithm>
#include <cassert>
#include <stdexcept>
#include <string>
#include <vector>

using eu_digital::CanonicalEvent;
using eu_digital::ImageReference;
using eu_digital::ImageStore;
using eu_digital::OcrEngine;
using eu_digital::OcrResult;
using eu_digital::OcrWord;
using eu_digital::ScreenFrame;
using eu_digital::ScreenOcrConfig;
using eu_digital::ScreenOcrSensor;
using eu_digital::ScreenRegion;

class MemoryImageStore final : public ImageStore {
public:
    int calls{0};

    ImageReference store(const ScreenFrame&, const std::string& perceptual_hash) override {
        ++calls;
        return {"captures/frame-" + perceptual_hash + ".bin", "sha256-placeholder"};
    }
};

class FakeOcr final : public OcrEngine {
public:
    int calls{0};
    bool fail{false};
    std::optional<ScreenRegion> received_region;

    OcrResult recognize(const ScreenFrame&, const std::optional<ScreenRegion>& region) override {
        ++calls;
        received_region = region;
        if (fail) throw std::runtime_error("local OCR unavailable");
        return {{{"Hello", {10, 20, 30, 12}, 0.95}}};
    }
};

ScreenFrame frame(std::uint64_t timestamp_ms, std::uint8_t tweak = 0) {
    std::vector<std::uint8_t> pixels(64, 100);
    pixels[0] = static_cast<std::uint8_t>(100 + tweak);
    return {timestamp_ms, 8, 8, std::move(pixels)};
}

ScreenFrame changed_frame(std::uint64_t timestamp_ms) {
    std::vector<std::uint8_t> pixels(64, 100);
    std::fill(pixels.begin(), pixels.begin() + 32, 200);
    return {timestamp_ms, 8, 8, std::move(pixels)};
}

int main() {
    MemoryImageStore store;
    FakeOcr ocr;
    std::vector<CanonicalEvent> events;
    ScreenOcrConfig config;
    config.perceptual_hamming_threshold = 4;
    config.region_of_interest = ScreenRegion{1, 2, 100, 80};
    ScreenOcrSensor sensor(store, ocr, [&](const CanonicalEvent& event) { events.push_back(event); }, config);

    assert(sensor.descriptor().capability_id == "screen.ocr");
    assert(sensor.observe(frame(10)));
    assert(events.size() == 2);
    assert(events[0].event_type == "screen.visual_captured");
    assert(events[1].event_type == "screen.ocr");
    assert(events[0].payload.find("captures/frame-") != std::string::npos);
    assert(events[0].payload.find("pixels") == std::string::npos);
    assert(events[1].payload.find("Hello") != std::string::npos);
    assert(events[1].payload.find("\"x\":10") != std::string::npos);
    assert(ocr.received_region.has_value());
    assert(ocr.received_region->width == 100);

    assert(!sensor.observe(frame(20, 1)));
    assert(ocr.calls == 1);
    assert(store.calls == 1);

    assert(sensor.observe(changed_frame(30)));
    assert(ocr.calls == 2);
    assert(store.calls == 2);

    assert(sensor.observe(changed_frame(5'030)));
    assert(ocr.calls == 3);
    assert(store.calls == 3);

    MemoryImageStore failing_store;
    FakeOcr failing_ocr;
    failing_ocr.fail = true;
    std::vector<CanonicalEvent> failure_events;
    ScreenOcrSensor failing_sensor(
        failing_store, failing_ocr,
        [&](const CanonicalEvent& event) { failure_events.push_back(event); });
    assert(failing_sensor.observe(frame(30)));
    assert(failure_events.size() == 1);
    assert(failure_events[0].event_type == "screen.visual_captured");
    assert(failing_sensor.health().ocr_failures == 1);
    assert(!failing_sensor.health().available);
}
