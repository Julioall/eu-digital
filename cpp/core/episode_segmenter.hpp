#pragma once

#include "core/capability_runtime.hpp"
#include "core/digest.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace eu_digital {

struct EpisodeSegmentEvent {
    std::string event_id;
    std::string session_id;
    std::string occurred_at;
    double epoch_seconds{0.0};
    std::optional<std::string> application;
    std::optional<std::string> document;
    std::string modality{"unknown"};
};

struct EpisodeSegmentConfig {
    double max_gap_seconds{300.0};
    bool split_on_application_change{true};
    bool split_on_document_change{true};

    void validate() const {
        if (!std::isfinite(max_gap_seconds) || max_gap_seconds <= 0.0) {
            throw std::invalid_argument("max_gap_seconds must be finite and positive");
        }
    }

    std::string fingerprint() const {
        validate();
        std::ostringstream number;
        number << std::setprecision(15) << max_gap_seconds;
        if (number.str().find('.') == std::string::npos && number.str().find('e') == std::string::npos) {
            number << ".0";
        }
        const std::string canonical =
            "{\"max_gap_seconds\":" + number.str() +
            ",\"split_on_application_change\":" +
            (split_on_application_change ? "true" : "false") +
            ",\"split_on_document_change\":" +
            (split_on_document_change ? "true" : "false") + "}";
        return digest::hex(digest::sha256(canonical)).substr(0, 16);
    }
};

struct EpisodeBoundary {
    std::string event_id;
    std::vector<std::string> reasons;
};

struct EpisodeRecord {
    std::string episode_id;
    std::string schema_version{"1.0"};
    std::string session_id;
    std::string start_at;
    std::string end_at;
    std::vector<std::string> event_ids;
    std::vector<std::string> applications;
    std::vector<std::string> documents;
    std::vector<std::string> modalities;
    std::vector<std::string> boundary_reasons;
};

struct EpisodeSegmentationResult {
    std::string schema_version{"1.0"};
    std::string baseline_id{"time_context_threshold_v1"};
    std::string config_fingerprint;
    std::vector<EpisodeRecord> episodes;
    std::vector<EpisodeBoundary> boundaries;
};

class EpisodeSegmenter {
public:
    static EpisodeSegmentationResult segment(
        const std::vector<EpisodeSegmentEvent>& events,
        const EpisodeSegmentConfig& config = {}) {
        config.validate();
        EpisodeSegmentationResult result;
        result.config_fingerprint = config.fingerprint();
        if (events.empty()) return result;

        const std::string& session_id = events.front().session_id;
        if (session_id.empty()) throw std::invalid_argument("session_id is required");
        std::vector<std::string> ids;
        ids.reserve(events.size());
        for (const auto& event : events) {
            if (event.event_id.empty() || event.session_id.empty()) {
                throw std::invalid_argument("event_id and session_id are required");
            }
            if (event.session_id != session_id) {
                throw std::invalid_argument("events from multiple sessions cannot be segmented together");
            }
            if (!std::isfinite(event.epoch_seconds)) {
                throw std::invalid_argument("event time must be finite");
            }
            if (!ids.empty() && event.epoch_seconds < events[ids.size() - 1].epoch_seconds) {
                throw std::invalid_argument("events must be ordered by occurred_at");
            }
            ids.push_back(event.event_id);
        }
        std::sort(ids.begin(), ids.end());
        if (std::adjacent_find(ids.begin(), ids.end()) != ids.end()) {
            throw std::invalid_argument("event_ids must be unique");
        }

        result.boundaries.push_back({events.front().event_id, {"episode_start"}});
        struct Range { std::size_t start; std::size_t end; std::vector<std::string> reasons; };
        std::vector<Range> ranges;
        std::size_t start = 0;
        const std::string* previous_application = events.front().application ? &*events.front().application : nullptr;
        const std::string* previous_document = events.front().document ? &*events.front().document : nullptr;

        for (std::size_t index = 1; index < events.size(); ++index) {
            const auto& current = events[index];
            const auto& previous = events[index - 1];
            std::vector<std::string> reasons;
            if (current.epoch_seconds - previous.epoch_seconds > config.max_gap_seconds) {
                reasons.emplace_back("time_gap");
            }
            if (config.split_on_application_change && current.application && previous_application &&
                *current.application != *previous_application) {
                reasons.emplace_back("context_change:application");
            }
            if (config.split_on_document_change && current.document && previous_document &&
                *current.document != *previous_document) {
                reasons.emplace_back("context_change:document");
            }
            if (!reasons.empty()) {
                result.boundaries.push_back({current.event_id, reasons});
                ranges.push_back({start, index, result.boundaries[result.boundaries.size() - 2].reasons});
                start = index;
            }
            if (current.application) previous_application = &*current.application;
            if (current.document) previous_document = &*current.document;
        }
        ranges.push_back({start, events.size(), result.boundaries.back().reasons});
        for (const auto& range : ranges) {
            result.episodes.push_back(make_episode(session_id, events, range.start, range.end, range.reasons, config));
        }
        return result;
    }

private:
    static EpisodeRecord make_episode(
        const std::string& session_id,
        const std::vector<EpisodeSegmentEvent>& events,
        std::size_t start,
        std::size_t end,
        std::vector<std::string> reasons,
        const EpisodeSegmentConfig& config) {
        EpisodeRecord episode;
        episode.session_id = session_id;
        episode.start_at = events[start].occurred_at;
        episode.end_at = events[end - 1].occurred_at;
        episode.boundary_reasons = std::move(reasons);
        episode.episode_id = digest::uuid5(
            "4f254c43-59a0-48bc-9e17-0f145f9ecac4",
            session_id + ":" + config.fingerprint() + ":" + events[start].event_id);
        for (std::size_t index = start; index < end; ++index) {
            const auto& event = events[index];
            episode.event_ids.push_back(event.event_id);
            if (event.application) episode.applications.push_back(*event.application);
            if (event.document) episode.documents.push_back(*event.document);
            episode.modalities.push_back(event.modality.empty() ? "unknown" : event.modality);
        }
        unique_sort(episode.applications);
        unique_sort(episode.documents);
        unique_sort(episode.modalities);
        return episode;
    }

    static void unique_sort(std::vector<std::string>& values) {
        std::sort(values.begin(), values.end());
        values.erase(std::unique(values.begin(), values.end()), values.end());
    }
};

class EpisodeSegmentationPlugin final : public CapabilityPlugin {
public:
    EpisodeSegmentationPlugin() {
        descriptor_.capability_id = "cognition.episode_segmentation";
        descriptor_.implementation_id = "native.episode_segmenter";
        descriptor_.implementation_version = "1.0.0";
        descriptor_.kind = "cognitive_service";
        descriptor_.provides.push_back({"segment.episodes", "urn:eu-digital:episode:1"});
        descriptor_.supports_hot_plug = true;
        descriptor_.supports_checkpoint = false;
    }

    const CapabilityDescriptor& descriptor() const override { return descriptor_; }
    void validate_manifest() override {}
    void configure() override {}
    void initialize() override {}
    void calibrate() override {}
    bool health_check() override { return true; }
    void start() override {}
    void drain() override {}
    std::map<std::string, std::string> checkpoint() override { return {}; }
    void stop() override {}
    void uninstall() override {}

private:
    CapabilityDescriptor descriptor_;
};

}  // namespace eu_digital
