#pragma once

#include "capability_runtime.hpp"
#include "event_bus.hpp"

#include <chrono>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace eu_digital {

struct AudioReference {
    std::string uri;
    std::string content_hash;
};

struct AudioFrame {
    std::uint64_t timestamp_ms{};
    std::uint64_t duration_ms{};
    AudioReference raw_audio;
    std::vector<float> samples;
};

struct VadResult {
    bool speech_detected{false};
    double speech_confidence{};
    std::string algorithm{"unknown"};
};

struct TranscriptionResult {
    std::string text;
    double confidence{};
};

class AudioCapture {
public:
    virtual ~AudioCapture() = default;
    virtual std::optional<AudioFrame> capture() = 0;
};

class VoiceActivityDetector {
public:
    virtual ~VoiceActivityDetector() = default;
    virtual VadResult detect(const AudioFrame& frame) = 0;
};

class LocalSpeechToText {
public:
    virtual ~LocalSpeechToText() = default;
    virtual TranscriptionResult transcribe(const AudioFrame& frame) = 0;
};

struct AudioSensorConfig {
    bool transcription_enabled{true};
};

enum class AudioObservationStatus { observed, no_signal, no_speech, sensor_failed };

struct AudioSensorHealth {
    bool available{true};
    std::uint64_t frames_processed{};
    std::uint64_t segments_emitted{};
    std::uint64_t no_signal_frames{};
    std::uint64_t no_speech_frames{};
    std::uint64_t transcription_failures{};
    double last_processing_cost_ms{};
    AudioObservationStatus last_status{AudioObservationStatus::no_signal};
    std::string last_error;
};

class AudioSensor {
public:
    using EventSink = std::function<void(const CanonicalEvent&)>;

    AudioSensor(VoiceActivityDetector& vad, LocalSpeechToText& speech_to_text,
                EventSink event_sink, AudioSensorConfig config = {})
        : vad_(vad), speech_to_text_(speech_to_text), event_sink_(std::move(event_sink)),
          config_(std::move(config)) {
        descriptor_.capability_id = "audio.speech";
        descriptor_.implementation_id = "local.audio_sensor";
        descriptor_.implementation_version = "1.0.0";
        descriptor_.kind = "sensor";
        descriptor_.provides.push_back({"observe.audio", "urn:eu-digital:audio-segment:1"});
        descriptor_.supports_hot_plug = true;
        descriptor_.supports_checkpoint = false;
        descriptor_.permissions = {"microphone.capture", "audio.raw.local_reference", "stt.local"};
    }

    const CapabilityDescriptor& descriptor() const { return descriptor_; }
    const AudioSensorConfig& config() const { return config_; }
    const AudioSensorHealth& health() const { return health_; }
    bool health_check() const { return health_.available; }

    AudioObservationStatus poll(AudioCapture& capture) {
        try {
            return observe(capture.capture());
        } catch (const std::exception& error) {
            return fail("audio capture failure: " + std::string(error.what()), 0);
        }
    }

    AudioObservationStatus observe(const std::optional<AudioFrame>& frame) {
        const auto started = std::chrono::steady_clock::now();
        if (!frame) {
            ++health_.no_signal_frames;
            health_.last_status = AudioObservationStatus::no_signal;
            health_.available = true;
            health_.last_error.clear();
            health_.last_processing_cost_ms = elapsed_ms(started);
            emit("audio.no_signal", "{\"schema_version\":\"1.0\",\"status\":\"no_signal\",\"processing_cost_ms\":" +
                number(health_.last_processing_cost_ms) + "}", 0);
            return health_.last_status;
        }

        if (!valid(*frame)) return fail("invalid audio frame", frame->timestamp_ms);
        ++health_.frames_processed;

        VadResult vad;
        try {
            vad = vad_.detect(*frame);
        } catch (const std::exception& error) {
            return fail("VAD failure: " + std::string(error.what()), frame->timestamp_ms);
        }
        if (!vad.speech_detected) {
            ++health_.no_speech_frames;
            health_.last_status = AudioObservationStatus::no_speech;
            health_.available = true;
            health_.last_error.clear();
            health_.last_processing_cost_ms = elapsed_ms(started);
            emit("audio.no_speech", "{\"schema_version\":\"1.0\",\"status\":\"no_speech\",\"speech_confidence\":" +
                number(vad.speech_confidence) + ",\"processing_cost_ms\":" + number(health_.last_processing_cost_ms) + "}",
                frame->timestamp_ms);
            return health_.last_status;
        }

        const std::string segment_id = "audio-segment-" + std::to_string(next_event_id_);
        const std::uint64_t end_timestamp_ms = frame->timestamp_ms + frame->duration_ms;
        const double segment_cost_ms = elapsed_ms(started);
        std::ostringstream segment_payload;
        segment_payload << "{\"schema_version\":\"1.0\",\"segment_id\":\""
                        << segment_id << "\",\"start_timestamp_ms\":" << frame->timestamp_ms
                        << ",\"end_timestamp_ms\":" << end_timestamp_ms
                        << ",\"audio_reference\":{\"uri\":\"" << json_escape(frame->raw_audio.uri)
                        << "\",\"content_hash\":\"" << json_escape(frame->raw_audio.content_hash)
                        << "\",\"duration_ms\":" << frame->duration_ms << "},\"vad\":{\"algorithm\":\""
                        << json_escape(vad.algorithm) << "\",\"speech_confidence\":"
                        << number(vad.speech_confidence) << "},\"processing_cost_ms\":"
                        << number(segment_cost_ms) << "}";
        emit("audio.segment", segment_payload.str(), frame->timestamp_ms);
        ++health_.segments_emitted;

        health_.last_status = AudioObservationStatus::observed;
        health_.available = true;
        health_.last_error.clear();
        if (config_.transcription_enabled) {
            const auto transcription_started = std::chrono::steady_clock::now();
            try {
                const auto result = speech_to_text_.transcribe(*frame);
                const double cost_ms = elapsed_ms(transcription_started);
                std::ostringstream payload;
                payload << "{\"schema_version\":\"1.0\",\"segment_id\":\"" << segment_id
                        << "\",\"start_timestamp_ms\":" << frame->timestamp_ms
                        << ",\"end_timestamp_ms\":" << end_timestamp_ms
                        << ",\"status\":\"transcribed\",\"text\":\"" << json_escape(result.text)
                        << "\",\"confidence\":" << number(result.confidence)
                        << ",\"processing_cost_ms\":" << number(cost_ms) << "}";
                emit("audio.transcription", payload.str(), frame->timestamp_ms);
                health_.last_processing_cost_ms = segment_cost_ms + cost_ms;
            } catch (const std::exception& error) {
                ++health_.transcription_failures;
                health_.available = false;
                health_.last_error = "transcription failure: " + std::string(error.what());
                const double cost_ms = elapsed_ms(transcription_started);
                std::ostringstream payload;
                payload << "{\"schema_version\":\"1.0\",\"segment_id\":\"" << segment_id
                        << "\",\"start_timestamp_ms\":" << frame->timestamp_ms
                        << ",\"end_timestamp_ms\":" << end_timestamp_ms
                        << ",\"status\":\"failed\",\"text\":null,\"confidence\":null"
                        << ",\"error_code\":\"transcription_failed\",\"processing_cost_ms\":"
                        << number(cost_ms) << "}";
                emit("audio.transcription_failed", payload.str(), frame->timestamp_ms);
                health_.last_processing_cost_ms = segment_cost_ms + cost_ms;
            }
        } else {
            health_.last_processing_cost_ms = segment_cost_ms;
        }
        return health_.last_status;
    }

private:
    static bool valid(const AudioFrame& frame) {
        return frame.duration_ms > 0 && !frame.raw_audio.uri.empty() && !frame.raw_audio.content_hash.empty();
    }

    static std::string json_escape(const std::string& value) {
        std::string escaped;
        for (const char character : value) {
            if (character == '\\' || character == '"') escaped.push_back('\\');
            escaped.push_back(character);
        }
        return escaped;
    }

    static std::string number(double value) {
        std::ostringstream output;
        output << value;
        return output.str();
    }

    static double elapsed_ms(const std::chrono::steady_clock::time_point started) {
        return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
    }

    AudioObservationStatus fail(const std::string& error, std::uint64_t timestamp_ms) {
        health_.available = false;
        health_.last_status = AudioObservationStatus::sensor_failed;
        health_.last_error = error;
        emit("audio.sensor_failed", "{\"schema_version\":\"1.0\",\"status\":\"sensor_failed\",\"error\":\"" +
            json_escape(error) + "}" , timestamp_ms);
        return health_.last_status;
    }

    void emit(const std::string& event_type, const std::string& payload, std::uint64_t timestamp_ms) {
        if (!event_sink_) return;
        CanonicalEvent event;
        event.event_id = "audio-sensor-" + std::to_string(next_event_id_++);
        event.source = "audio_sensor";
        event.event_type = event_type;
        event.payload = payload;
        event.monotonic_ns = timestamp_ms * 1'000'000;
        event_sink_(event);
    }

    VoiceActivityDetector& vad_;
    LocalSpeechToText& speech_to_text_;
    EventSink event_sink_;
    AudioSensorConfig config_;
    CapabilityDescriptor descriptor_;
    AudioSensorHealth health_;
    std::uint64_t next_event_id_{1};
};

class AudioSensorPlugin final : public CapabilityPlugin {
public:
    AudioSensorPlugin(AudioCapture& capture, VoiceActivityDetector& vad, LocalSpeechToText& speech_to_text,
                      AudioSensor::EventSink event_sink, AudioSensorConfig config = {})
        : capture_(capture), sensor_(vad, speech_to_text, std::move(event_sink), std::move(config)) {}

    const CapabilityDescriptor& descriptor() const override { return sensor_.descriptor(); }
    void validate_manifest() override {
        if (!descriptor().valid()) throw CapabilityLifecycleError("invalid audio descriptor");
    }
    void configure() override {}
    void initialize() override {}
    void calibrate() override {}
    bool health_check() override { return sensor_.health_check(); }
    void start() override {}
    void drain() override {}
    std::map<std::string, std::string> checkpoint() override { return {}; }
    void stop() override {}
    void uninstall() override {}

    AudioObservationStatus poll() { return sensor_.poll(capture_); }
    const AudioSensorHealth& health() const { return sensor_.health(); }

private:
    AudioCapture& capture_;
    AudioSensor sensor_;
};

}  // namespace eu_digital
