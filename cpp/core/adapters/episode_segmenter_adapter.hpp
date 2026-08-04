#pragma once

#include "core/episode_segmenter.hpp"
#include "core/ports/icognitive_state_port.hpp"
#include "core/ports/iepisode_boundary_port.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <iomanip>
#include <limits>
#include <map>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace eu_digital {

class EpisodeSegmenterAdapter final : public IEpisodeBoundaryPort,
                                      public ICognitiveStatePort {
public:
    explicit EpisodeSegmenterAdapter(
        std::string provider_id = "episode_boundary_impl")
        : provider_id_(std::move(provider_id)) {
        if (provider_id_.empty()) {
            throw std::invalid_argument("episode state provider_id is required");
        }
    }

    EpisodeUpdate evaluate(const CanonicalEvent&) override {
        throw std::invalid_argument(
            "legacy canonical event lacks episode observation fields");
    }

    EpisodeUpdate evaluate_observation(
        const contracts::EpisodeObservationRequest& request) override {
        if (!request.valid()) {
            throw std::invalid_argument("invalid episode observation request");
        }
        std::lock_guard lock(mutex_);

        EpisodeSegmentEvent event;
        event.event_id = request.event_id;
        event.session_id = request.session_id;
        event.occurred_at = request.occurred_at;
        event.epoch_seconds = request.epoch_seconds;
        event.application = request.application;
        event.document = request.document;
        event.modality = request.modality;

        auto& events = events_by_session_[request.session_id];
        events.push_back(std::move(event));
        const auto result = EpisodeSegmenter::segment(events);

        EpisodeUpdate update;
        update.episode_id = result.episodes.back().episode_id;
        update.is_new_episode = std::any_of(
            result.boundaries.begin(), result.boundaries.end(),
            [&](const auto& boundary) {
                return boundary.event_id == request.event_id;
            });
        update.current_state = "active";
        return update;
    }

    std::string provider_id() const override { return provider_id_; }

    std::string state_schema_version() const override { return "1.0"; }

    contracts::PortResult<contracts::CognitiveStateFragmentV1>
    capture_state(
        const contracts::PortInvocationContextV1& context) const override {
        if (context.stop_requested()) {
            return contracts::PortResult<
                contracts::CognitiveStateFragmentV1>::failed(
                    "cognitive_state.capture", "cancelled",
                    "checkpoint capture was cancelled", true);
        }
        std::map<std::string, std::vector<EpisodeSegmentEvent>> state;
        {
            std::lock_guard lock(mutex_);
            state = events_by_session_;
        }
        contracts::CognitiveStateFragmentV1 fragment;
        fragment.provider_id = provider_id_;
        fragment.state_schema_version = state_schema_version();
        fragment.entries["session_count"] = std::to_string(state.size());
        std::size_t session_index = 0;
        for (const auto& [session_id, events] : state) {
            const auto session_prefix =
                "session." + std::to_string(session_index);
            fragment.entries[session_prefix + ".id"] = session_id;
            fragment.entries[session_prefix + ".event_count"] =
                std::to_string(events.size());
            for (std::size_t event_index = 0; event_index < events.size();
                 ++event_index) {
                const auto prefix = session_prefix + ".event." +
                    std::to_string(event_index);
                const auto& event = events[event_index];
                fragment.entries[prefix + ".event_id"] = event.event_id;
                fragment.entries[prefix + ".occurred_at"] = event.occurred_at;
                fragment.entries[prefix + ".epoch_seconds"] =
                    format_number(event.epoch_seconds);
                fragment.entries[prefix + ".application_present"] =
                    event.application ? "1" : "0";
                fragment.entries[prefix + ".application"] =
                    event.application.value_or("");
                fragment.entries[prefix + ".document_present"] =
                    event.document ? "1" : "0";
                fragment.entries[prefix + ".document"] =
                    event.document.value_or("");
                fragment.entries[prefix + ".modality"] = event.modality;
            }
            ++session_index;
        }
        if (context.stop_requested()) {
            return contracts::PortResult<
                contracts::CognitiveStateFragmentV1>::failed(
                    "cognitive_state.capture", "cancelled",
                    "checkpoint capture exceeded its deadline", true);
        }
        return contracts::PortResult<
            contracts::CognitiveStateFragmentV1>::ok(std::move(fragment));
    }

    contracts::PortResult<contracts::CognitiveStateRestoreResultV1>
    restore_state(
        const contracts::CognitiveStateFragmentV1& fragment,
        const contracts::PortInvocationContextV1& context) override {
        constexpr auto operation = "cognitive_state.restore";
        if (context.stop_requested()) {
            return contracts::PortResult<
                contracts::CognitiveStateRestoreResultV1>::failed(
                    operation, "cancelled", "state restore was cancelled", true);
        }
        if (!fragment.valid() || fragment.provider_id != provider_id_ ||
            fragment.state_schema_version != state_schema_version()) {
            return contracts::PortResult<
                contracts::CognitiveStateRestoreResultV1>::failed(
                    operation, "incompatible_state_fragment",
                    "episode state provider or schema does not match");
        }
        try {
            auto entries = fragment.entries;
            std::map<std::string, std::vector<EpisodeSegmentEvent>> restored;
            const auto session_count = parse_size(take(entries, "session_count"));
            for (std::size_t session_index = 0;
                 session_index < session_count; ++session_index) {
                const auto session_prefix =
                    "session." + std::to_string(session_index);
                const auto session_id = take(entries, session_prefix + ".id");
                if (session_id.empty()) {
                    throw std::invalid_argument("empty restored session_id");
                }
                const auto event_count = parse_size(
                    take(entries, session_prefix + ".event_count"));
                if (event_count == 0) {
                    throw std::invalid_argument("empty restored episode session");
                }
                auto& events = restored[session_id];
                events.reserve(event_count);
                for (std::size_t event_index = 0; event_index < event_count;
                     ++event_index) {
                    const auto prefix = session_prefix + ".event." +
                        std::to_string(event_index);
                    EpisodeSegmentEvent event;
                    event.session_id = session_id;
                    event.event_id = take(entries, prefix + ".event_id");
                    event.occurred_at = take(entries, prefix + ".occurred_at");
                    event.epoch_seconds = parse_number(
                        take(entries, prefix + ".epoch_seconds"));
                    event.application = parse_optional(
                        take(entries, prefix + ".application_present"),
                        take(entries, prefix + ".application"));
                    event.document = parse_optional(
                        take(entries, prefix + ".document_present"),
                        take(entries, prefix + ".document"));
                    event.modality = take(entries, prefix + ".modality");
                    events.push_back(std::move(event));
                }
                // Reuse the canonical segmentation invariants before mutating
                // live state.
                (void)EpisodeSegmenter::segment(events);
            }
            if (!entries.empty()) {
                throw std::invalid_argument("unknown episode state entries");
            }
            if (context.stop_requested()) {
                return contracts::PortResult<
                    contracts::CognitiveStateRestoreResultV1>::failed(
                        operation, "cancelled",
                        "state restore exceeded its deadline", true);
            }
            {
                std::lock_guard lock(mutex_);
                events_by_session_.swap(restored);
            }
            contracts::CognitiveStateRestoreResultV1 result;
            result.provider_id = provider_id_;
            result.restored_entries = fragment.entries.size();
            return contracts::PortResult<
                contracts::CognitiveStateRestoreResultV1>::ok(
                    std::move(result));
        } catch (const std::exception& error) {
            return contracts::PortResult<
                contracts::CognitiveStateRestoreResultV1>::failed(
                    operation, "invalid_state_fragment", error.what());
        }
    }

private:
    static std::string format_number(double value) {
        if (!std::isfinite(value)) {
            throw std::invalid_argument("episode time must be finite");
        }
        std::ostringstream output;
        output << std::setprecision(std::numeric_limits<double>::max_digits10)
               << value;
        return output.str();
    }

    static std::string take(std::map<std::string, std::string>& entries,
                            const std::string& key) {
        const auto found = entries.find(key);
        if (found == entries.end()) {
            throw std::invalid_argument("missing episode state entry: " + key);
        }
        auto value = std::move(found->second);
        entries.erase(found);
        return value;
    }

    static std::size_t parse_size(const std::string& value) {
        std::size_t parsed = 0;
        const auto [end, error] = std::from_chars(
            value.data(), value.data() + value.size(), parsed);
        if (error != std::errc{} || end != value.data() + value.size()) {
            throw std::invalid_argument("invalid episode state count");
        }
        return parsed;
    }

    static double parse_number(const std::string& value) {
        std::size_t parsed = 0;
        const auto number = std::stod(value, &parsed);
        if (parsed != value.size() || !std::isfinite(number)) {
            throw std::invalid_argument("invalid episode state time");
        }
        return number;
    }

    static std::optional<std::string> parse_optional(
        const std::string& present, std::string value) {
        if (present == "0" && value.empty()) return std::nullopt;
        if (present == "1" && !value.empty()) return value;
        throw std::invalid_argument("invalid optional episode state value");
    }

    const std::string provider_id_;
    mutable std::mutex mutex_;
    std::map<std::string, std::vector<EpisodeSegmentEvent>> events_by_session_;
};

}  // namespace eu_digital
