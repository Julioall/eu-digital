#pragma once

#include "core/contracts/port_result.hpp"

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace eu_digital::contracts {

struct PatternLearningObservation {
    std::map<std::string, double> features;
    std::string observation_ref;
    double occurred_epoch{0.0};
};

struct PatternLearningFeedback {
    std::string pattern_id;
    bool positive{false};
    std::string reference;
};

struct ObservedPattern {
    std::string pattern_id;
    std::string schema_version{"1.0"};
    int version{1};
    std::string status{"candidate"};
    std::map<std::string, double> centroid;
    int support{1};
    double stability{0.0};
    double recency{1.0};
    double confidence{0.5};
    std::vector<std::string> observation_refs;
    int positive_feedback{0};
    int negative_feedback{0};
    std::vector<std::string> feedback_references;
    std::optional<std::string> parent_pattern_id;
    std::optional<std::string> drift_reason;
    std::string created_by;
};

using PatternLearningResult = PortResult<ObservedPattern>;

}  // namespace eu_digital::contracts
