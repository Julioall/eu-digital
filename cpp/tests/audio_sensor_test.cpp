#include "core/audio_sensor.hpp"

#include <cassert>
#include <stdexcept>
#include <string>
#include <vector>

using eu_digital::AudioCapture;
using eu_digital::AudioFrame;
using eu_digital::AudioObservationStatus;
using eu_digital::AudioSensor;
using eu_digital::LocalSpeechToText;
using eu_digital::TranscriptionResult;
using eu_digital::VadResult;
using eu_digital::VoiceActivityDetector;

namespace {

AudioFrame frame(std::uint64_t timestamp_ms = 1000) {
    return {timestamp_ms, 250, {"audio://local/segment-1.wav", "sha256:segment-1"}, {0.1F, 0.2F, 0.3F}};
}

class FakeVad final : public VoiceActivityDetector {
public:
    VadResult result{true, 0.91, "fixture-vad"};
    bool throw_error{false};

    VadResult detect(const AudioFrame&) override {
        if (throw_error) throw std::runtime_error("vad unavailable");
        return result;
    }
};

class FakeSpeechToText final : public LocalSpeechToText {
public:
    bool throw_error{false};
    int calls{};

    TranscriptionResult transcribe(const AudioFrame&) override {
        ++calls;
        if (throw_error) throw std::runtime_error("model unavailable");
        return {"reuniao local", 0.88};
    }
};

class FakeCapture final : public AudioCapture {
public:
    std::optional<AudioFrame> next;
    bool throw_error{false};

    std::optional<AudioFrame> capture() override {
        if (throw_error) throw std::runtime_error("permission denied");
        return next;
    }
};

}  // namespace

int main() {
    FakeVad vad;
    FakeSpeechToText speech_to_text;
    std::vector<eu_digital::CanonicalEvent> events;
    AudioSensor sensor(vad, speech_to_text, [&](const eu_digital::CanonicalEvent& event) {
        events.push_back(event);
    });

    assert(sensor.descriptor().valid());
    assert(sensor.descriptor().provides_operation("observe.audio"));
    assert(sensor.observe(frame()) == AudioObservationStatus::observed);
    assert(events.size() == 2);
    assert(events[0].event_type == "audio.segment");
    assert(events[0].monotonic_ns == 1'000'000'000);
    assert(events[0].payload.find("start_timestamp_ms\":1000") != std::string::npos);
    assert(events[0].payload.find("end_timestamp_ms\":1250") != std::string::npos);
    assert(events[0].payload.find("audio://local") != std::string::npos);
    assert(events[0].payload.find("0.1") == std::string::npos);
    assert(events[0].payload.find("processing_cost_ms") != std::string::npos);
    assert(events[1].event_type == "audio.transcription");
    assert(speech_to_text.calls == 1);

    const auto events_before_failure = events.size();
    speech_to_text.throw_error = true;
    assert(sensor.observe(frame(2000)) == AudioObservationStatus::observed);
    assert(events.size() == events_before_failure + 2);
    assert(events.back().event_type == "audio.transcription_failed");
    assert(sensor.health().segments_emitted == 2);
    assert(sensor.health().transcription_failures == 1);
    assert(sensor.health().last_processing_cost_ms >= 0.0);

    vad.result.speech_detected = false;
    assert(sensor.observe(frame(3000)) == AudioObservationStatus::no_speech);
    assert(events.back().event_type == "audio.no_speech");
    assert(sensor.health().no_speech_frames == 1);

    assert(sensor.observe(std::nullopt) == AudioObservationStatus::no_signal);
    assert(events.back().event_type == "audio.no_signal");
    assert(sensor.health().no_signal_frames == 1);

    vad.result.speech_detected = true;
    vad.throw_error = true;
    assert(sensor.observe(frame(4000)) == AudioObservationStatus::sensor_failed);
    assert(events.back().event_type == "audio.sensor_failed");
    assert(!sensor.health_check());

    FakeCapture capture;
    capture.next = frame(5000);
    FakeVad healthy_vad;
    FakeSpeechToText healthy_speech;
    AudioSensor polled(healthy_vad, healthy_speech, {});
    assert(polled.poll(capture) == AudioObservationStatus::observed);
    capture.next = std::nullopt;
    assert(polled.poll(capture) == AudioObservationStatus::no_signal);
    capture.throw_error = true;
    assert(polled.poll(capture) == AudioObservationStatus::sensor_failed);

    return 0;
}
