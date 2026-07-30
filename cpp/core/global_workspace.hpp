#pragma once

#include "core/capability_runtime.hpp"
#include "core/digest.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace eu_digital {

inline constexpr const char* WORKSPACE_SCHEMA_VERSION = "1.0";
inline constexpr const char* WORKSPACE_POLICY_ID = "observed_weighted_mean_v1";
inline constexpr const char* WORKSPACE_BASELINE_ID = "fifo_capacity_v0";
inline constexpr const char* WORKSPACE_CREATED_BY = "global_workspace.observed_weighted_mean.v1";
inline constexpr const char* WORKSPACE_NAMESPACE = "f835ced2-e6e2-4d16-a414-e5bd3c931c86";

inline const std::vector<std::string>& workspace_salient_factors() {
    static const std::vector<std::string> factors{
        "novelty", "surprise", "repetition", "conflict", "direct_mention",
        "goal_relevance", "risk", "learning_opportunity", "ignore_cost", "priority"};
    return factors;
}

inline std::string workspace_escape_json(const std::string& value) {
    std::ostringstream output;
    for (const auto character : value) {
        switch (character) {
        case '"': output << "\\\""; break;
        case '\\': output << "\\\\"; break;
        case '\b': output << "\\b"; break;
        case '\f': output << "\\f"; break;
        case '\n': output << "\\n"; break;
        case '\r': output << "\\r"; break;
        case '\t': output << "\\t"; break;
        default: output << character; break;
        }
    }
    return output.str();
}

inline std::string workspace_python_float(double value) {
    if (!std::isfinite(value)) throw std::invalid_argument("workspace number must be finite");
    std::array<char, 64> buffer{};
    const auto conversion = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
    if (conversion.ec != std::errc{}) throw std::invalid_argument("workspace number cannot be formatted");
    auto result = std::string(buffer.data(), conversion.ptr);
    if (result.find_first_of(".eE") == std::string::npos) result += ".0";
    return result;
}

inline std::int64_t workspace_floor_seconds(double epoch) {
    return static_cast<std::int64_t>(std::floor(epoch));
}

inline std::string workspace_format_utc(double epoch) {
    const auto seconds = workspace_floor_seconds(epoch);
    const auto days = seconds / 86400 - (seconds < 0 && seconds % 86400 != 0 ? 1 : 0);
    const auto day_seconds = seconds - days * 86400;
    std::int64_t z = days + 719468;
    const std::int64_t era = (z >= 0 ? z : z - 146096) / 146097;
    const auto day_of_era = static_cast<unsigned>(z - era * 146097);
    const auto year_of_era = (day_of_era - day_of_era / 1460 + day_of_era / 36524 - day_of_era / 146096) / 365;
    std::int64_t year = static_cast<std::int64_t>(year_of_era) + era * 400;
    const auto day_of_year = day_of_era - (365 * year_of_era + year_of_era / 4 - year_of_era / 100);
    const auto month_part = (5 * day_of_year + 2) / 153;
    const auto day = day_of_year - (153 * month_part + 2) / 5 + 1;
    const auto month = month_part + (month_part < 10 ? 3 : -9);
    year += month <= 2;
    const auto hour = day_seconds / 3600;
    const auto minute = (day_seconds % 3600) / 60;
    const auto second = day_seconds % 60;
    std::ostringstream output;
    output << std::setfill('0') << std::setw(4) << year << '-' << std::setw(2) << month << '-' << std::setw(2) << day
           << 'T' << std::setw(2) << hour << ':' << std::setw(2) << minute << ':' << std::setw(2) << second << "+00:00";
    return output.str();
}

struct WorkspaceCandidate {
    std::string candidate_id;
    std::string session_id;
    std::string source_kind;
    std::vector<std::string> source_refs;
    std::string observed_at;
    std::map<std::string, std::string> content;
    std::map<std::string, double> salience_signals;
    std::string schema_version{WORKSPACE_SCHEMA_VERSION};

    void validate() const {
        if (candidate_id.empty() || session_id.empty()) throw std::invalid_argument("workspace candidate names are required");
        if (schema_version != WORKSPACE_SCHEMA_VERSION) throw std::invalid_argument("unsupported workspace candidate schema version");
        if (source_kind != "canonical_event" && source_kind != "episode" && source_kind != "pattern" && source_kind != "internal") {
            throw std::invalid_argument("unsupported workspace candidate source_kind");
        }
        if (source_refs.empty() || std::any_of(source_refs.begin(), source_refs.end(), [](const auto& value) { return value.empty(); })) {
            throw std::invalid_argument("workspace candidate source_refs are required");
        }
        if (observed_at.empty() || salience_signals.empty()) throw std::invalid_argument("workspace candidate observations are required");
        for (const auto& [name, value] : salience_signals) {
            if (name.empty() || !std::isfinite(value) || value < 0.0 || value > 1.0) {
                throw std::invalid_argument("workspace salience signals must be bounded and named");
            }
        }
    }
};

struct WorkspaceConfig {
    int capacity{4};
    double ttl_seconds{120.0};
    int max_candidates{256};
    std::string selection_policy{WORKSPACE_POLICY_ID};
    std::map<std::string, double> weights{
        {"conflict", 1.2}, {"direct_mention", 1.2}, {"goal_relevance", 1.1},
        {"ignore_cost", 1.0}, {"learning_opportunity", 0.8}, {"novelty", 1.0},
        {"priority", 1.3}, {"repetition", 0.5}, {"risk", 1.3}, {"surprise", 1.0}};
    std::set<std::string> enabled_factors{
        "conflict", "direct_mention", "goal_relevance", "ignore_cost", "learning_opportunity",
        "novelty", "priority", "repetition", "risk", "surprise"};

    void validate() const {
        if (capacity <= 0 || max_candidates < capacity) throw std::invalid_argument("workspace capacity bound is invalid");
        if (!std::isfinite(ttl_seconds) || ttl_seconds <= 0.0) throw std::invalid_argument("workspace ttl is invalid");
        if (selection_policy != WORKSPACE_POLICY_ID && selection_policy != WORKSPACE_BASELINE_ID) {
            throw std::invalid_argument("unsupported workspace selection policy");
        }
        if (weights.size() != workspace_salient_factors().size()) throw std::invalid_argument("workspace weights are incomplete");
        for (const auto& name : workspace_salient_factors()) {
            const auto found = weights.find(name);
            if (found == weights.end() || !std::isfinite(found->second) || found->second <= 0.0) {
                throw std::invalid_argument("workspace weights are invalid");
            }
        }
        for (const auto& name : enabled_factors) {
            if (!weights.contains(name)) throw std::invalid_argument("workspace enabled factor is unsupported");
        }
    }

    std::string fingerprint() const {
        validate();
        std::ostringstream canonical;
        canonical << "{\"capacity\":" << capacity << ",\"enabled_factors\":[";
        std::size_t index = 0;
        for (const auto& name : enabled_factors) {
            if (index++) canonical << ',';
            canonical << '"' << workspace_escape_json(name) << '"';
        }
        canonical << "],\"max_candidates\":" << max_candidates << ",\"selection_policy\":\""
                  << workspace_escape_json(selection_policy) << "\",\"ttl_seconds\":" << workspace_python_float(ttl_seconds)
                  << ",\"weights\":{";
        index = 0;
        for (const auto& [name, weight] : weights) {
            if (index++) canonical << ',';
            canonical << '"' << workspace_escape_json(name) << "\":" << workspace_python_float(weight);
        }
        canonical << "}}";
        return digest::hex(digest::sha256(canonical.str())).substr(0, 16);
    }
};

struct WorkspaceSalience {
    std::string policy_id;
    double score{0.0};
    std::map<std::string, double> observed_factors;
    std::vector<std::string> missing_factors;
};

struct WorkspaceDecision {
    std::string candidate_id;
    std::optional<double> score;
    bool selected{false};
    std::optional<int> rank;
    std::vector<std::string> reason_codes;
};

struct WorkspaceItem {
    std::string workspace_item_id;
    std::string schema_version{WORKSPACE_SCHEMA_VERSION};
    std::string workspace_id;
    std::string candidate_id;
    std::string session_id;
    std::string source_kind;
    std::vector<std::string> source_refs;
    std::string observed_at;
    std::string admitted_at;
    std::string expires_at;
    std::map<std::string, std::string> content;
    WorkspaceSalience salience;
    std::string snapshot_id;
    int rank{1};
    std::string selected_at;
    std::vector<std::string> selection_reasons;
};

struct WorkspaceSnapshot {
    std::string snapshot_id;
    std::string schema_version{WORKSPACE_SCHEMA_VERSION};
    std::string workspace_id;
    std::string session_id;
    std::string created_at;
    int capacity{1};
    std::string policy_id;
    std::string config_fingerprint;
    double selection_churn{0.0};
    std::vector<WorkspaceItem> active_items;
    std::vector<WorkspaceDecision> decisions;
    std::vector<std::string> expired_candidate_ids;
    std::vector<std::string> discarded_candidate_ids;
};

struct WorkspaceBroadcast {
    std::string broadcast_id;
    std::string schema_version{WORKSPACE_SCHEMA_VERSION};
    std::string workspace_id;
    std::string session_id;
    std::string emitted_at;
    WorkspaceSnapshot snapshot;
};

class GlobalWorkspace {
public:
    GlobalWorkspace(std::string workspace_id, std::string session_id, WorkspaceConfig config)
        : workspace_id_(std::move(workspace_id)), session_id_(std::move(session_id)), config_(std::move(config)) {
        if (workspace_id_.empty() || session_id_.empty()) throw std::invalid_argument("workspace names are required");
        config_.validate();
    }

    WorkspaceSnapshot admit(WorkspaceCandidate candidate, const std::string& now, double now_epoch) {
        candidate.validate();
        const auto expired = expire(now_epoch);
        if (candidate.session_id != session_id_) throw std::invalid_argument("candidate session_id does not match workspace");
        const auto found = entries_.find(candidate.candidate_id);
        if (found != entries_.end()) {
            if (!same_candidate(found->second.candidate, candidate)) throw std::invalid_argument("candidate_id is immutable");
            return build_snapshot(now, now_epoch, expired, {});
        }
        Entry entry;
        entry.candidate = std::move(candidate);
        entry.workspace_item_id = digest::uuid5(WORKSPACE_NAMESPACE, workspace_id_ + ":" + session_id_ + ":" + entry.candidate.candidate_id);
        entry.admitted_epoch = now_epoch;
        entry.admitted_at = now;
        entry.expires_epoch = now_epoch + config_.ttl_seconds;
        entries_[entry.candidate.candidate_id] = std::move(entry);
        return build_snapshot(now, now_epoch, expired, enforce_resource_bound());
    }

    WorkspaceSnapshot update_priority(const std::string& candidate_id, double priority, const std::string& now, double now_epoch) {
        const auto expired = expire(now_epoch);
        if (!std::isfinite(priority) || priority < 0.0 || priority > 1.0) throw std::invalid_argument("priority is invalid");
        const auto found = entries_.find(candidate_id);
        if (found == entries_.end()) throw std::invalid_argument("candidate is unavailable for priority update");
        found->second.candidate.salience_signals["priority"] = priority;
        return build_snapshot(now, now_epoch, expired, {});
    }

    WorkspaceSnapshot snapshot(const std::string& now, double now_epoch) {
        return build_snapshot(now, now_epoch, expire(now_epoch), {});
    }

    WorkspaceBroadcast broadcast(const WorkspaceSnapshot& snapshot, const std::string& emitted_at, double emitted_epoch) const {
        if (snapshot.workspace_id != workspace_id_ || snapshot.session_id != session_id_) {
            throw std::invalid_argument("snapshot does not belong to workspace");
        }
        return WorkspaceBroadcast{
            digest::uuid5(WORKSPACE_NAMESPACE, snapshot.snapshot_id + ":" + emitted_at),
            WORKSPACE_SCHEMA_VERSION,
            workspace_id_,
            session_id_,
            emitted_at,
            snapshot,
        };
    }

    const WorkspaceConfig& config() const { return config_; }
    const std::string& workspace_id() const { return workspace_id_; }
    const std::string& session_id() const { return session_id_; }

private:
    struct Entry {
        WorkspaceCandidate candidate;
        std::string workspace_item_id;
        double admitted_epoch{0.0};
        std::string admitted_at;
        double expires_epoch{0.0};
    };

    struct RankedEntry {
        std::string candidate_id;
        std::optional<WorkspaceSalience> salience;
    };

    static bool same_candidate(const WorkspaceCandidate& left, const WorkspaceCandidate& right) {
        return left.candidate_id == right.candidate_id && left.session_id == right.session_id &&
            left.source_kind == right.source_kind && left.source_refs == right.source_refs &&
            left.observed_at == right.observed_at && left.content == right.content &&
            left.salience_signals == right.salience_signals && left.schema_version == right.schema_version;
    }

    std::vector<std::string> expire(double now_epoch) {
        std::vector<std::string> expired;
        for (auto iterator = entries_.begin(); iterator != entries_.end();) {
            if (iterator->second.expires_epoch <= now_epoch) {
                expired.push_back(iterator->first);
                iterator = entries_.erase(iterator);
            } else {
                ++iterator;
            }
        }
        return expired;
    }

    std::vector<std::string> enforce_resource_bound() {
        if (entries_.size() <= static_cast<std::size_t>(config_.max_candidates)) return {};
        const auto ranked = rank_entries();
        std::set<std::string> retained;
        for (std::size_t index = 0; index < static_cast<std::size_t>(config_.max_candidates) && index < ranked.size(); ++index) {
            retained.insert(ranked[index].candidate_id);
        }
        std::vector<std::string> discarded;
        for (auto iterator = entries_.begin(); iterator != entries_.end();) {
            if (!retained.contains(iterator->first)) {
                discarded.push_back(iterator->first);
                iterator = entries_.erase(iterator);
            } else {
                ++iterator;
            }
        }
        return discarded;
    }

    std::optional<WorkspaceSalience> assess(const Entry& entry) const {
        if (config_.selection_policy == WORKSPACE_BASELINE_ID) {
            return WorkspaceSalience{WORKSPACE_BASELINE_ID, 0.0, {}, sorted_missing_factors()};
        }
        WorkspaceSalience result;
        result.policy_id = WORKSPACE_POLICY_ID;
        double denominator = 0.0;
        for (const auto& name : config_.enabled_factors) {
            const auto found = entry.candidate.salience_signals.find(name);
            if (found == entry.candidate.salience_signals.end()) {
                result.missing_factors.push_back(name);
            } else {
                result.observed_factors[name] = found->second;
                denominator += config_.weights.at(name);
                result.score += found->second * config_.weights.at(name);
            }
        }
        if (result.observed_factors.empty()) return std::nullopt;
        result.score /= denominator;
        return result;
    }

    std::vector<std::string> sorted_missing_factors() const {
        return std::vector<std::string>(config_.enabled_factors.begin(), config_.enabled_factors.end());
    }

    std::vector<RankedEntry> rank_entries() const {
        std::vector<RankedEntry> ranked;
        for (const auto& [candidate_id, entry] : entries_) ranked.push_back({candidate_id, assess(entry)});
        if (config_.selection_policy == WORKSPACE_BASELINE_ID) {
            std::sort(ranked.begin(), ranked.end(), [&](const auto& left, const auto& right) {
                const auto& left_entry = entries_.at(left.candidate_id);
                const auto& right_entry = entries_.at(right.candidate_id);
                if (left_entry.admitted_epoch != right_entry.admitted_epoch) return left_entry.admitted_epoch < right_entry.admitted_epoch;
                return left.candidate_id < right.candidate_id;
            });
            return ranked;
        }
        std::sort(ranked.begin(), ranked.end(), [](const auto& left, const auto& right) {
            const bool left_unscored = !left.salience.has_value();
            const bool right_unscored = !right.salience.has_value();
            if (left_unscored != right_unscored) return !left_unscored;
            if (!left_unscored && left.salience->score != right.salience->score) return left.salience->score > right.salience->score;
            return left.candidate_id < right.candidate_id;
        });
        return ranked;
    }

    static std::string json_string(const std::string& value) { return "\"" + workspace_escape_json(value) + "\""; }

    static std::string json_array(const std::vector<std::string>& values) {
        std::ostringstream output;
        output << '[';
        for (std::size_t index = 0; index < values.size(); ++index) {
            if (index) output << ',';
            output << json_string(values[index]);
        }
        output << ']';
        return output.str();
    }

    static std::string canonical_decision(const WorkspaceDecision& decision) {
        std::ostringstream output;
        output << "{\"candidate_id\":" << json_string(decision.candidate_id)
               << ",\"rank\":" << (decision.rank ? std::to_string(*decision.rank) : "null")
               << ",\"reason_codes\":" << json_array(decision.reason_codes)
               << ",\"score\":" << (decision.score ? workspace_python_float(*decision.score) : "null")
               << ",\"selected\":" << (decision.selected ? "true" : "false") << '}';
        return output.str();
    }

    std::string snapshot_id(const std::string& created_at,
                            const std::vector<WorkspaceDecision>& decisions,
                            const std::vector<std::string>& expired,
                            const std::vector<std::string>& discarded,
                            double churn) const {
        std::ostringstream canonical;
        canonical << "{\"config_fingerprint\":" << json_string(config_.fingerprint())
                  << ",\"created_at\":" << json_string(created_at) << ",\"decisions\":[";
        for (std::size_t index = 0; index < decisions.size(); ++index) {
            if (index) canonical << ',';
            canonical << canonical_decision(decisions[index]);
        }
        canonical << "],\"discarded\":" << json_array(discarded)
                  << ",\"expired\":" << json_array(expired)
                  << ",\"selection_churn\":" << workspace_python_float(churn)
                  << ",\"session_id\":" << json_string(session_id_)
                  << ",\"workspace_id\":" << json_string(workspace_id_) << '}';
        return digest::uuid5(WORKSPACE_NAMESPACE, canonical.str());
    }

    WorkspaceSnapshot build_snapshot(const std::string& now, double now_epoch,
                                     const std::vector<std::string>& expired,
                                     const std::vector<std::string>& discarded) {
        const auto ranked = rank_entries();
        std::set<std::string> selected_ids;
        std::vector<std::pair<std::string, WorkspaceSalience>> selected;
        for (const auto& item : ranked) {
            if (item.salience && selected.size() < static_cast<std::size_t>(config_.capacity)) {
                selected_ids.insert(item.candidate_id);
                selected.emplace_back(item.candidate_id, *item.salience);
            }
        }
        std::vector<WorkspaceDecision> decisions;
        for (std::size_t index = 0; index < ranked.size(); ++index) {
            const auto& ranked_item = ranked[index];
            WorkspaceDecision decision;
            decision.candidate_id = ranked_item.candidate_id;
            decision.score = ranked_item.salience ? std::optional<double>(ranked_item.salience->score) : std::nullopt;
            if (!ranked_item.salience) {
                decision.reason_codes = {"unscored:no_observed_enabled_factor"};
            } else {
                decision.rank = static_cast<int>(index + 1);
                decision.selected = selected_ids.contains(ranked_item.candidate_id);
                for (const auto& [factor, unused] : ranked_item.salience->observed_factors) {
                    decision.reason_codes.push_back("salience.observed:" + factor);
                }
                decision.reason_codes.insert(decision.reason_codes.begin(), "salience.policy:" + ranked_item.salience->policy_id);
                decision.reason_codes.push_back(decision.selected
                    ? (ranked_item.salience->policy_id == WORKSPACE_BASELINE_ID ? "selection.fifo_admission" : "selection.capacity")
                    : "capacity.excluded");
            }
            decisions.push_back(std::move(decision));
        }
        std::set<std::string> combined = last_active_candidate_ids_;
        combined.insert(selected_ids.begin(), selected_ids.end());
        std::size_t symmetric = 0;
        for (const auto& id : combined) {
            if (last_active_candidate_ids_.contains(id) != selected_ids.contains(id)) ++symmetric;
        }
        const double churn = combined.empty() ? 0.0 : static_cast<double>(symmetric) / static_cast<double>(combined.size());
        const auto id = snapshot_id(now, decisions, expired, discarded, churn);
        std::vector<WorkspaceItem> active_items;
        for (std::size_t index = 0; index < selected.size(); ++index) {
            const auto& entry = entries_.at(selected[index].first);
            const auto decision = std::find_if(decisions.begin(), decisions.end(), [&](const auto& value) {
                return value.candidate_id == selected[index].first;
            });
            WorkspaceItem item;
            item.workspace_item_id = entry.workspace_item_id;
            item.workspace_id = workspace_id_;
            item.candidate_id = entry.candidate.candidate_id;
            item.session_id = entry.candidate.session_id;
            item.source_kind = entry.candidate.source_kind;
            item.source_refs = entry.candidate.source_refs;
            item.observed_at = entry.candidate.observed_at;
            item.admitted_at = entry.admitted_at;
            item.expires_at = workspace_format_utc(entry.expires_epoch);
            item.content = entry.candidate.content;
            item.salience = selected[index].second;
            item.snapshot_id = id;
            item.rank = static_cast<int>(index + 1);
            item.selected_at = now;
            item.selection_reasons = decision->reason_codes;
            active_items.push_back(std::move(item));
        }
        last_active_candidate_ids_ = std::move(selected_ids);
        return WorkspaceSnapshot{
            id, WORKSPACE_SCHEMA_VERSION, workspace_id_, session_id_, now, config_.capacity,
            config_.selection_policy, config_.fingerprint(), churn, std::move(active_items),
            std::move(decisions), expired, discarded};
    }

    std::string workspace_id_;
    std::string session_id_;
    WorkspaceConfig config_;
    std::map<std::string, Entry> entries_;
    std::set<std::string> last_active_candidate_ids_;
};

class GlobalWorkspacePlugin final : public CapabilityPlugin {
public:
    GlobalWorkspacePlugin() {
        descriptor_.capability_id = "cognition.global_workspace";
        descriptor_.implementation_id = "native.global_workspace";
        descriptor_.implementation_version = "1.0.0";
        descriptor_.kind = "cognitive_service";
        descriptor_.provides.push_back({"select.workspace", "urn:eu-digital:workspace:1"});
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
