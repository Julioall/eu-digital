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
using eu_digital::ScreenOcrCaptureMode;
using eu_digital::ScreenOcrCapturePolicy;
using eu_digital::ScreenOcrConsentState;
using eu_digital::ScreenOcrConfig;
using eu_digital::ScreenOcrPlugin;
using eu_digital::ScreenOcrSensor;
using eu_digital::ScreenOcrRequest;
using eu_digital::ScreenRegion;

class MemoryImageStore final : public ImageStore {
public:
    int calls{0};
    ScreenFrame last_frame;

    ImageReference store(const ScreenFrame& frame, const std::string& perceptual_hash) override {
        ++calls;
        last_frame = frame;
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
    std::vector<std::uint8_t> pixels(64, 0);
    pixels[17] = 255;
    return {timestamp_ms, 8, 8, std::move(pixels)};
}

int main() {
    MemoryImageStore denied_store;
    FakeOcr denied_ocr;
    std::vector<CanonicalEvent> denied_events;
    ScreenOcrSensor denied_sensor(
        denied_store, denied_ocr,
        [&](const CanonicalEvent& event) { denied_events.push_back(event); });
    assert(!denied_sensor.observe(frame(1)));
    assert(denied_events.empty());
    assert(denied_store.calls == 0);
    assert(denied_sensor.health().last_suppression_reason == "consent_not_granted");

    ScreenOcrCapturePolicy policy;
    policy.policy_id = "screen.ocr.test";
    policy.consent_state = ScreenOcrConsentState::granted;
    policy.capture_mode = ScreenOcrCaptureMode::explicit_enable;
    policy.region_of_interest = ScreenRegion{1, 2, 4, 3};
    MemoryImageStore store;
    FakeOcr ocr;
    std::vector<CanonicalEvent> events;
    ScreenOcrConfig config;
    config.perceptual_hamming_threshold = 4;
    config.policy = policy;
    ScreenOcrSensor sensor(store, ocr, [&](const CanonicalEvent& event) { events.push_back(event); }, config);

    assert(sensor.descriptor().capability_id == "screen.ocr");
    assert(sensor.descriptor().supports_hot_plug);
    assert(sensor.observe(frame(10)));
    assert(events.size() == 2);
    assert(events[0].event_type == "screen.visual_captured");
    assert(events[1].event_type == "screen.ocr");
    assert(events[0].payload.find("captures/frame-") != std::string::npos);
    assert(events[0].payload.find("pixels") == std::string::npos);
    assert(events[0].payload.find("reference-and-hash-only") != std::string::npos);
    assert(events[0].payload.find("\"width\":4") != std::string::npos);
    assert(events[1].payload.find("Hello") == std::string::npos);
    assert(events[1].payload.find("redacted:length=5") != std::string::npos);
    assert(events[1].payload.find("\"retention_days\":7") != std::string::npos);
    assert(events[1].payload.find("length-only-v1") != std::string::npos);
    assert(events[1].payload.find("\"x\":10") != std::string::npos);
    assert(ocr.received_region.has_value());
    assert(ocr.received_region->width == 4);
    assert(store.last_frame.width == 4);
    assert(store.last_frame.height == 3);
    assert(store.last_frame.pixels.size() == 12);

    ScreenOcrCapturePolicy revoked_policy = policy;
    revoked_policy.consent_state = ScreenOcrConsentState::revoked;
    revoked_policy.capture_mode = ScreenOcrCaptureMode::disabled;
    sensor.set_policy(revoked_policy);
    assert(!sensor.observe(frame(15)));
    assert(sensor.health().last_suppression_reason == "consent_not_granted");
    assert(store.calls == 1);
    sensor.set_policy(policy);

    assert(!sensor.observe(frame(20, 1)));
    assert(ocr.calls == 1);
    assert(store.calls == 1);

    assert(sensor.observe(changed_frame(30)));
    assert(ocr.calls == 2);
    assert(store.calls == 2);

    assert(sensor.observe(changed_frame(5'030)));
    assert(ocr.calls == 3);
    assert(store.calls == 3);

    ScreenOcrCapturePolicy request_policy = policy;
    request_policy.capture_mode = ScreenOcrCaptureMode::on_demand;
    request_policy.request_id = "request-1";
    ScreenOcrConfig request_config;
    request_config.policy = request_policy;
    MemoryImageStore request_store;
    FakeOcr request_ocr;
    std::vector<CanonicalEvent> request_events;
    ScreenOcrSensor request_sensor(
        request_store, request_ocr,
        [&](const CanonicalEvent& event) { request_events.push_back(event); }, request_config);
    assert(!request_sensor.observe(frame(40)));
    assert(!request_sensor.observe(frame(50), ScreenOcrRequest{"wrong", std::nullopt}));
    assert(request_sensor.observe(frame(60), ScreenOcrRequest{"request-1", std::nullopt}));
    assert(request_events[0].payload.find("capture_authorization\":\"on_demand") != std::string::npos);
    assert(request_events[0].payload.find("request-1") != std::string::npos);

    ScreenOcrCapturePolicy paused_policy = policy;
    paused_policy.global_pause = true;
    ScreenOcrConfig paused_config;
    paused_config.policy = paused_policy;
    MemoryImageStore paused_store;
    FakeOcr paused_ocr;
    ScreenOcrSensor paused_sensor(paused_store, paused_ocr, {} , paused_config);
    assert(!paused_sensor.observe(frame(70)));
    assert(paused_sensor.health().paused);
    assert(paused_sensor.health().last_suppression_reason == "global_pause");
    assert(paused_store.calls == 0);

    ScreenOcrCapturePolicy invalid_region_policy = policy;
    invalid_region_policy.region_of_interest = ScreenRegion{7, 7, 2, 2};
    ScreenOcrConfig invalid_region_config;
    invalid_region_config.policy = invalid_region_policy;
    MemoryImageStore invalid_region_store;
    FakeOcr invalid_region_ocr;
    ScreenOcrSensor invalid_region_sensor(invalid_region_store, invalid_region_ocr, {}, invalid_region_config);
    assert(!invalid_region_sensor.observe(frame(80)));
    assert(invalid_region_store.calls == 0);
    assert(invalid_region_sensor.health().last_error.find("outside frame") != std::string::npos);

    MemoryImageStore failing_store;
    FakeOcr failing_ocr;
    failing_ocr.fail = true;
    std::vector<CanonicalEvent> failure_events;
    ScreenOcrSensor failing_sensor(
        failing_store, failing_ocr,
        [&](const CanonicalEvent& event) { failure_events.push_back(event); },
        ScreenOcrConfig{4, 5'000, policy, true});
    assert(failing_sensor.observe(frame(30)));
    assert(failure_events.size() == 2);
    assert(failure_events[0].event_type == "screen.visual_captured");
    assert(failure_events[1].event_type == "screen.ocr_unavailable");
    assert(failure_events[1].payload.find("text_observation_status\":\"unavailable") != std::string::npos);
    assert(failure_events[1].payload.find("words") == std::string::npos);
    assert(failing_sensor.health().ocr_failures == 1);
    assert(!failing_sensor.health().ocr_available);
    assert(!failing_sensor.health().available);

    ScreenOcrPlugin plugin(store, ocr, {}, config);
    plugin.validate_manifest();
    assert(plugin.descriptor().supports_hot_plug);
    plugin.uninstall();
}
