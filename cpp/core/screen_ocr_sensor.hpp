#pragma once

#include "capability_runtime.hpp"
#include "event_bus.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <functional>
#include <map>
#include <numeric>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace eu_digital {

struct ScreenRegion {
    int x{};
    int y{};
    int width{};
    int height{};

    bool operator==(const ScreenRegion&) const = default;
};

struct ScreenFrame {
    std::uint64_t timestamp_ms{};
    std::uint32_t width{};
    std::uint32_t height{};
    std::vector<std::uint8_t> pixels;
};

struct ImageReference {
    std::string path;
    std::string content_hash;
};

class ImageStore {
public:
    virtual ~ImageStore() = default;
    virtual ImageReference store(const ScreenFrame& frame, const std::string& perceptual_hash) = 0;
};

struct OcrWord {
    std::string text;
    ScreenRegion bounds;
    double confidence{};
};

struct OcrResult {
    std::vector<OcrWord> words;
};

class OcrEngine {
public:
    virtual ~OcrEngine() = default;
    virtual OcrResult recognize(const ScreenFrame& frame,
                                const std::optional<ScreenRegion>& region) = 0;
};

struct ScreenOcrConfig {
    // Average-hash distance at or below this value is considered redundant.
    std::uint32_t perceptual_hamming_threshold{4};
    // Similar frames are sampled again after this interval.
    std::uint64_t capture_interval_ms{5'000};
    std::optional<ScreenRegion> region_of_interest;
    bool ocr_enabled{true};
};

struct ScreenOcrHealth {
    bool available{true};
    std::uint64_t skipped_redundant{};
    std::uint32_t ocr_failures{};
    std::string last_error;
};

class ScreenOcrSensor {
public:
    using EventSink = std::function<void(const CanonicalEvent&)>;

    ScreenOcrSensor(ImageStore& image_store, OcrEngine& ocr_engine, EventSink event_sink,
                    ScreenOcrConfig config = {})
        : image_store_(image_store), ocr_engine_(ocr_engine), event_sink_(std::move(event_sink)),
          config_(std::move(config)) {
        descriptor_.capability_id = "screen.ocr";
        descriptor_.implementation_id = "local.screen_ocr";
        descriptor_.implementation_version = "1.0.0";
        descriptor_.kind = "sensor";
        descriptor_.provides.push_back({"observe.screen", "urn:eu-digital:screen-ocr:1"});
        descriptor_.supports_hot_plug = true;
        descriptor_.supports_checkpoint = false;
        descriptor_.permissions = {"screen.capture", "ocr.local"};
    }

    const CapabilityDescriptor& descriptor() const { return descriptor_; }
    const ScreenOcrConfig& config() const { return config_; }
    const ScreenOcrHealth& health() const { return health_; }
    bool health_check() const { return health_.available; }

    // Returns true when a visual capture was accepted, including when OCR fails.
    bool observe(const ScreenFrame& frame) {
        if (!valid_frame(frame)) {
            health_.available = false;
            health_.last_error = "invalid screen frame";
            return false;
        }

        const std::uint64_t hash = average_hash(frame);
        if (previous_hash_ && hamming_distance(*previous_hash_, hash) <=
                config_.perceptual_hamming_threshold &&
            frame.timestamp_ms < last_capture_ms_ + config_.capture_interval_ms) {
            ++health_.skipped_redundant;
            return false;
        }

        ImageReference image;
        try {
            image = image_store_.store(frame, hash_string(hash));
        } catch (const std::exception& error) {
            health_.available = false;
            health_.last_error = std::string("image store failure: ") + error.what();
            return false;
        }

        previous_hash_ = hash;
        last_capture_ms_ = frame.timestamp_ms;
        health_.available = true;
        health_.last_error.clear();
        emit_visual(frame, image, hash_string(hash));

        if (!config_.ocr_enabled) return true;
        try {
            const OcrResult result = ocr_engine_.recognize(frame, config_.region_of_interest);
            emit_ocr(frame, image, result);
        } catch (const std::exception& error) {
            health_.available = false;
            ++health_.ocr_failures;
            health_.last_error = std::string("OCR failure: ") + error.what();
        }
        return true;
    }

    static std::uint64_t average_hash(const ScreenFrame& frame) {
        if (frame.pixels.empty()) throw std::invalid_argument("cannot hash an empty screen frame");
        const std::uint64_t total = std::accumulate(frame.pixels.begin(), frame.pixels.end(), std::uint64_t{0});
        const std::uint8_t average = static_cast<std::uint8_t>(total / frame.pixels.size());
        const std::size_t sample_count = std::min<std::size_t>(64, frame.pixels.size());
        std::uint64_t hash = 0;
        for (std::size_t index = 0; index < sample_count; ++index) {
            const std::size_t pixel_index = index * frame.pixels.size() / sample_count;
            if (frame.pixels[pixel_index] >= average) hash |= std::uint64_t{1} << index;
        }
        return hash;
    }

private:
    static bool valid_frame(const ScreenFrame& frame) {
        if (frame.width == 0 || frame.height == 0 || frame.pixels.empty()) return false;
        const std::uint64_t expected = static_cast<std::uint64_t>(frame.width) * frame.height;
        return expected == frame.pixels.size();
    }

    static std::uint32_t hamming_distance(std::uint64_t left, std::uint64_t right) {
        std::uint64_t different = left ^ right;
        std::uint32_t distance = 0;
        while (different != 0) {
            different &= different - 1;
            ++distance;
        }
        return distance;
    }

    static std::string hash_string(std::uint64_t hash) {
        std::ostringstream output;
        output << std::hex << hash;
        return output.str();
    }

    static std::string json_escape(const std::string& value) {
        std::string escaped;
        for (const char character : value) {
            if (character == '\\' || character == '"') escaped.push_back('\\');
            escaped.push_back(character);
        }
        return escaped;
    }

    static void append_region(std::ostringstream& payload, const ScreenRegion& region) {
        payload << "{\"x\":" << region.x << ",\"y\":" << region.y
                << ",\"width\":" << region.width << ",\"height\":" << region.height << "}";
    }

    void emit_visual(const ScreenFrame& frame, const ImageReference& image,
                     const std::string& perceptual_hash) {
        std::ostringstream payload;
        payload << "{\"payload_schema_version\":\"1.0\",\"image_path\":\""
                << json_escape(image.path) << "\",\"content_hash\":\""
                << json_escape(image.content_hash) << "\",\"perceptual_hash\":\""
                << perceptual_hash << "\",\"width\":" << frame.width
                << ",\"height\":" << frame.height;
        if (config_.region_of_interest) {
            payload << ",\"region_of_interest\":";
            append_region(payload, *config_.region_of_interest);
        }
        payload << "}";
        emit("screen.visual_captured", payload.str(), frame.timestamp_ms);
    }

    void emit_ocr(const ScreenFrame& frame, const ImageReference& image, const OcrResult& result) {
        std::ostringstream payload;
        payload << "{\"payload_schema_version\":\"1.0\",\"image_path\":\""
                << json_escape(image.path) << "\",\"content_hash\":\""
                << json_escape(image.content_hash) << "\",\"words\":[";
        for (std::size_t index = 0; index < result.words.size(); ++index) {
            if (index != 0) payload << ',';
            const auto& word = result.words[index];
            payload << "{\"text\":\"" << json_escape(word.text) << "\",\"x\":"
                    << word.bounds.x << ",\"y\":" << word.bounds.y
                    << ",\"width\":" << word.bounds.width << ",\"height\":"
                    << word.bounds.height << ",\"confidence\":" << word.confidence << "}";
        }
        payload << "],\"timestamp_ms\":" << frame.timestamp_ms << "}";
        emit("screen.ocr", payload.str(), frame.timestamp_ms);
    }

    void emit(const std::string& event_type, const std::string& payload, std::uint64_t timestamp_ms) {
        if (!event_sink_) return;
        CanonicalEvent event;
        event.event_id = "screen-ocr-" + std::to_string(next_event_id_++);
        event.source = "screen_ocr_sensor";
        event.event_type = event_type;
        event.payload = payload;
        event.monotonic_ns = timestamp_ms * 1'000'000;
        event_sink_(event);
    }

    ImageStore& image_store_;
    OcrEngine& ocr_engine_;
    EventSink event_sink_;
    ScreenOcrConfig config_;
    CapabilityDescriptor descriptor_;
    ScreenOcrHealth health_;
    std::optional<std::uint64_t> previous_hash_;
    std::uint64_t last_capture_ms_{0};
    std::uint64_t next_event_id_{1};
};

class ScreenOcrPlugin final : public CapabilityPlugin {
public:
    ScreenOcrPlugin(ImageStore& image_store, OcrEngine& ocr_engine,
                    ScreenOcrSensor::EventSink event_sink, ScreenOcrConfig config = {})
        : sensor_(image_store, ocr_engine, std::move(event_sink), std::move(config)) {}

    const CapabilityDescriptor& descriptor() const override { return sensor_.descriptor(); }
    void validate_manifest() override {
        if (!descriptor().valid()) throw CapabilityLifecycleError("invalid screen OCR descriptor");
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

    bool observe(const ScreenFrame& frame) { return sensor_.observe(frame); }
    const ScreenOcrHealth& health() const { return sensor_.health(); }

private:
    ScreenOcrSensor sensor_;
};

}  // namespace eu_digital
