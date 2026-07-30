#pragma once

#include "core/capability_runtime.hpp"
#include "core/digest.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cctype>
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

inline constexpr const char* METACOGNITION_CURIOSITY_SCHEMA_VERSION = "1.0";
inline constexpr const char* METACOGNITION_RAW_CONFIDENCE_ID = "evidence_ratio_v1";
inline constexpr const char* METACOGNITION_CALIBRATOR_ID = "bucketed_beta_v1";
inline constexpr const char* METACOGNITION_INFORMATION_GAIN_POLICY_ID = "information_gain_v1";
inline constexpr const char* METACOGNITION_BASELINE_CONFIDENCE_ID = "raw_confidence_v0";
inline constexpr const char* METACOGNITION_BASELINE_QUESTION_POLICY_ID = "fixed_gain_v0";
inline constexpr const char* METACOGNITION_CREATED_BY = "metacognition_curiosity.local.v1";
inline constexpr const char* METACOGNITION_NAMESPACE = "26da5c19-e611-4ebb-a26e-5b341d2df708";
inline constexpr const char* METACOGNITION_HYPOTHESIS =
    "outcome-calibrated confidence and information-gain selection reduce unjustified and redundant question proposals versus fixed controls";
inline constexpr const char* METACOGNITION_ABLATION =
    "disable outcome calibration, select fixed_gain_v0, and disable budget, cooldown, or redundancy suppression";
inline constexpr const char* METACOGNITION_FALSIFICATION =
    "confidence remains decoupled from verified outcomes, or information-gain selection does not improve gain, redundancy, and interruption cost over controls";

class MetacognitionCuriosityError : public std::invalid_argument {
public:
    explicit MetacognitionCuriosityError(const std::string& message) : std::invalid_argument(message) {}
};

enum class HypothesisStatus { proposed, confirmed, rejected, superseded };
enum class QuestionPolicy { information_gain_v1, fixed_gain_v0 };
enum class QuestionStatus { proposed, suppressed, asked, answered };
enum class ResponseOutcome { confirmed, rejected, inconclusive };

inline std::string metacognition_json_escape(const std::string& value) {
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

inline std::string metacognition_json_string(const std::string& value) {
    return "\"" + metacognition_json_escape(value) + "\"";
}

inline std::string metacognition_json_number(double value) {
    if (!std::isfinite(value)) throw MetacognitionCuriosityError("non-finite number cannot be serialized");
    std::array<char, 64> buffer{};
    const auto conversion = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
    if (conversion.ec != std::errc{}) throw MetacognitionCuriosityError("number cannot be formatted");
    auto result = std::string(buffer.data(), conversion.ptr);
    if (result.find_first_of(".eE") == std::string::npos) result += ".0";
    return result;
}

inline std::string metacognition_json_array(const std::vector<std::string>& values) {
    std::ostringstream output;
    output << '[';
    for (std::size_t index = 0; index < values.size(); ++index) {
        if (index) output << ',';
        output << metacognition_json_string(values[index]);
    }
    output << ']';
    return output.str();
}

inline void metacognition_required_string(const std::string& value, const char* name) {
    if (value.empty()) throw MetacognitionCuriosityError(std::string(name) + " must be a non-empty string");
}

inline void metacognition_probability(double value, const char* name) {
    if (!std::isfinite(value) || value < 0.0 || value > 1.0) {
        throw MetacognitionCuriosityError(std::string(name) + " must be a finite number between zero and one");
    }
}

inline std::int64_t metacognition_days_from_civil(int year, unsigned month, unsigned day) {
    year -= month <= 2;
    const int era = (year >= 0 ? year : year - 399) / 400;
    const unsigned year_of_era = static_cast<unsigned>(year - era * 400);
    const unsigned day_of_year = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
    const unsigned day_of_era = year_of_era * 365 + year_of_era / 4 - year_of_era / 100 + day_of_year;
    return static_cast<std::int64_t>(era) * 146097 + static_cast<std::int64_t>(day_of_era) - 719468;
}

inline double metacognition_parse_timestamp(const std::string& value) {
    if (value.size() < 20 || value[4] != '-' || value[7] != '-' || value[10] != 'T') {
        throw MetacognitionCuriosityError("timestamp must be a valid ISO-8601 string");
    }
    const auto number = [&](std::size_t offset, std::size_t length) {
        try { return std::stoi(value.substr(offset, length)); }
        catch (const std::exception&) { throw MetacognitionCuriosityError("timestamp must be a valid ISO-8601 string"); }
    };
    const int year = number(0, 4);
    const unsigned month = static_cast<unsigned>(number(5, 2));
    const unsigned day = static_cast<unsigned>(number(8, 2));
    const int hour = number(11, 2);
    const int minute = number(14, 2);
    const int second = number(17, 2);
    const auto zone_start = value.find_first_of("Z+-", 19);
    if (zone_start == std::string::npos) throw MetacognitionCuriosityError("timestamp must include timezone");
    double fraction = 0.0;
    if (zone_start > 19) {
        try { fraction = std::stod("0" + value.substr(19, zone_start - 19)); }
        catch (const std::exception&) { throw MetacognitionCuriosityError("timestamp must be a valid ISO-8601 string"); }
    }
    int offset_seconds = 0;
    if (value[zone_start] != 'Z') {
        if (value.size() < zone_start + 6 || value[zone_start + 3] != ':') {
            throw MetacognitionCuriosityError("timestamp timezone is invalid");
        }
        const int sign = value[zone_start] == '-' ? -1 : 1;
        offset_seconds = sign * (number(zone_start + 1, 2) * 3600 + number(zone_start + 4, 2) * 60);
    }
    return static_cast<double>(metacognition_days_from_civil(static_cast<int>(year), month, day) * 86400LL +
                               hour * 3600 + minute * 60 + second - offset_seconds) + fraction;
}

inline std::string metacognition_format_timestamp(double epoch) {
    const auto seconds = static_cast<std::int64_t>(std::floor(epoch));
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

inline std::string metacognition_normalize_timestamp(const std::string& value) {
    return metacognition_format_timestamp(metacognition_parse_timestamp(value));
}

inline std::string metacognition_question_fingerprint(const std::string& prompt) {
    std::ostringstream output;
    bool pending_space = false;
    for (const auto character : prompt) {
        const auto unsigned_character = static_cast<unsigned char>(character);
        if (std::isspace(unsigned_character)) {
            if (output.tellp() > 0) pending_space = true;
            continue;
        }
        if (pending_space) output << ' ';
        pending_space = false;
        output << static_cast<char>(std::tolower(unsigned_character));
    }
    return output.str();
}

inline std::string metacognition_tuple_repr(const std::vector<std::string>& values) {
    if (values.empty()) return "()";
    std::ostringstream output;
    output << '(';
    for (std::size_t index = 0; index < values.size(); ++index) {
        if (index) output << ", ";
        output << '\'';
        for (const auto character : values[index]) {
            if (character == '\\' || character == '\'') output << '\\';
            output << character;
        }
        output << '\'';
    }
    if (values.size() == 1) output << ',';
    output << ')';
    return output.str();
}

struct HypothesisRecord {
    std::string hypothesis_id;
    std::string kind;
    std::string statement;
    HypothesisStatus status{HypothesisStatus::proposed};
    double confidence{0.5};
    std::vector<std::string> supporting_refs;
    std::vector<std::string> opposing_refs;
    std::vector<std::string> alternatives;
    std::string created_at;
    std::string updated_at;
    std::optional<std::string> verification_question;
    std::optional<double> expected_information_gain;
    std::string provenance_module;
    std::optional<std::string> model_version;
    std::string schema_version{METACOGNITION_CURIOSITY_SCHEMA_VERSION};

    void validate() const {
        metacognition_required_string(hypothesis_id, "hypothesis_id");
        static const std::set<std::string> kinds{"observed_pattern", "causal", "predictive", "contextual", "capability"};
        if (!kinds.contains(kind)) throw MetacognitionCuriosityError("unsupported hypothesis kind");
        metacognition_required_string(statement, "statement");
        metacognition_probability(confidence, "confidence");
        metacognition_required_string(created_at, "created_at");
        metacognition_required_string(updated_at, "updated_at");
        if (metacognition_parse_timestamp(updated_at) < metacognition_parse_timestamp(created_at)) {
            throw MetacognitionCuriosityError("updated_at cannot precede created_at");
        }
        for (const auto* references : {&supporting_refs, &opposing_refs, &alternatives}) {
            if (std::any_of(references->begin(), references->end(), [](const auto& value) { return value.empty(); })) {
                throw MetacognitionCuriosityError("hypothesis references must be non-empty");
            }
        }
        if (verification_question) metacognition_required_string(*verification_question, "verification_question");
        if (expected_information_gain) metacognition_probability(*expected_information_gain, "expected_information_gain");
        metacognition_required_string(provenance_module, "provenance_module");
        if (model_version) metacognition_required_string(*model_version, "model_version");
        if (schema_version != METACOGNITION_CURIOSITY_SCHEMA_VERSION) throw MetacognitionCuriosityError("unsupported hypothesis schema version");
    }

    std::string to_json() const {
        validate();
        std::ostringstream output;
        output << "{\"alternatives\":" << metacognition_json_array(alternatives)
               << ",\"created_at\":" << metacognition_json_string(created_at)
               << ",\"evidence\":{\"opposing_refs\":" << metacognition_json_array(opposing_refs)
               << ",\"supporting_refs\":" << metacognition_json_array(supporting_refs) << "}"
               << ",\"hypothesis_id\":" << metacognition_json_string(hypothesis_id)
               << ",\"kind\":" << metacognition_json_string(kind)
               << ",\"provenance\":{\"model_version\":"
               << (model_version ? metacognition_json_string(*model_version) : "null")
               << ",\"module\":" << metacognition_json_string(provenance_module) << "}"
               << ",\"schema_version\":" << metacognition_json_string(schema_version)
               << ",\"statement\":" << metacognition_json_string(statement)
               << ",\"status\":" << metacognition_json_string(hypothesis_status_string(status))
               << ",\"updated_at\":" << metacognition_json_string(updated_at)
               << ",\"verification\":{\"expected_information_gain\":"
               << (expected_information_gain ? metacognition_json_number(*expected_information_gain) : "null")
               << ",\"question\":" << (verification_question ? metacognition_json_string(*verification_question) : "null") << "}"
               << ",\"confidence\":" << metacognition_json_number(confidence) << '}';
        return output.str();
    }

    static std::string hypothesis_status_string(HypothesisStatus value) {
        switch (value) {
        case HypothesisStatus::proposed: return "proposed";
        case HypothesisStatus::confirmed: return "confirmed";
        case HypothesisStatus::rejected: return "rejected";
        case HypothesisStatus::superseded: return "superseded";
        }
        throw MetacognitionCuriosityError("unsupported hypothesis status");
    }
};

struct CuriosityConfig {
    bool calibration_enabled{true};
    QuestionPolicy question_policy{QuestionPolicy::information_gain_v1};
    int interruptions_per_window{3};
    double interruption_window_seconds{900.0};
    double cooldown_seconds{300.0};
    double correction_cooldown_seconds{1800.0};
    double min_information_gain{0.1};
    double silence_confidence{0.9};
    bool redundancy_suppression_enabled{true};
    bool budget_enabled{true};
    bool cooldown_enabled{true};
    int calibration_bucket_count{10};

    void validate() const {
        if (interruptions_per_window <= 0) throw MetacognitionCuriosityError("interruptions_per_window must be positive");
        if (calibration_bucket_count <= 1) throw MetacognitionCuriosityError("calibration_bucket_count must exceed one");
        for (const auto value : {interruption_window_seconds, cooldown_seconds, correction_cooldown_seconds}) {
            if (!std::isfinite(value) || value < 0.0) throw MetacognitionCuriosityError("time configuration must be finite and non-negative");
        }
        metacognition_probability(min_information_gain, "min_information_gain");
        metacognition_probability(silence_confidence, "silence_confidence");
    }

    std::string policy_string() const { return question_policy == QuestionPolicy::information_gain_v1 ? METACOGNITION_INFORMATION_GAIN_POLICY_ID : METACOGNITION_BASELINE_QUESTION_POLICY_ID; }

    std::string fingerprint() const {
        validate();
        std::ostringstream value;
        value << "{\"budget_enabled\":" << (budget_enabled ? "true" : "false")
              << ",\"calibration_bucket_count\":" << calibration_bucket_count
              << ",\"calibration_enabled\":" << (calibration_enabled ? "true" : "false")
              << ",\"cooldown_enabled\":" << (cooldown_enabled ? "true" : "false")
              << ",\"cooldown_seconds\":" << metacognition_json_number(cooldown_seconds)
              << ",\"correction_cooldown_seconds\":" << metacognition_json_number(correction_cooldown_seconds)
              << ",\"interruption_window_seconds\":" << metacognition_json_number(interruption_window_seconds)
              << ",\"interruptions_per_window\":" << interruptions_per_window
              << ",\"min_information_gain\":" << metacognition_json_number(min_information_gain)
              << ",\"question_policy\":" << metacognition_json_string(policy_string())
              << ",\"redundancy_suppression_enabled\":" << (redundancy_suppression_enabled ? "true" : "false")
              << ",\"silence_confidence\":" << metacognition_json_number(silence_confidence) << '}';
        return digest::hex(digest::sha256(value.str())).substr(0, 16);
    }
};

struct MetacognitiveAssessment {
    std::string assessment_id;
    std::string schema_version{METACOGNITION_CURIOSITY_SCHEMA_VERSION};
    std::string hypothesis_id;
    std::string evaluated_at;
    double raw_confidence{0.0};
    double calibrated_confidence{0.0};
    std::optional<double> evidence_balance;
    double uncertainty{0.0};
    std::vector<std::string> supporting_refs;
    std::vector<std::string> opposing_refs;
    std::vector<std::string> alternatives;
    std::string decision;
    std::vector<std::string> reason_codes;
    std::string calibrator_id;

    std::string to_json() const {
        metacognition_required_string(assessment_id, "assessment_id");
        metacognition_required_string(hypothesis_id, "hypothesis_id");
        metacognition_probability(raw_confidence, "raw_confidence");
        metacognition_probability(calibrated_confidence, "calibrated_confidence");
        if (evidence_balance) metacognition_probability(*evidence_balance, "evidence_balance");
        metacognition_probability(uncertainty, "uncertainty");
        if (decision != "question" && decision != "silence") throw MetacognitionCuriosityError("unsupported assessment decision");
        std::ostringstream output;
        output << "{\"alternatives\":" << metacognition_json_array(alternatives)
               << ",\"assessment_id\":" << metacognition_json_string(assessment_id)
               << ",\"calibrated_confidence\":" << metacognition_json_number(calibrated_confidence)
               << ",\"calibrator_id\":" << metacognition_json_string(calibrator_id)
               << ",\"decision\":" << metacognition_json_string(decision)
               << ",\"evidence_balance\":" << (evidence_balance ? metacognition_json_number(*evidence_balance) : "null")
               << ",\"evaluated_at\":" << metacognition_json_string(evaluated_at)
               << ",\"hypothesis_id\":" << metacognition_json_string(hypothesis_id)
               << ",\"opposing_refs\":" << metacognition_json_array(opposing_refs)
               << ",\"raw_confidence\":" << metacognition_json_number(raw_confidence)
               << ",\"reason_codes\":" << metacognition_json_array(reason_codes)
               << ",\"schema_version\":" << metacognition_json_string(schema_version)
               << ",\"supporting_refs\":" << metacognition_json_array(supporting_refs)
               << ",\"uncertainty\":" << metacognition_json_number(uncertainty) << '}';
        return output.str();
    }
};

struct CuriosityQuestion {
    std::string question_id;
    std::string schema_version{METACOGNITION_CURIOSITY_SCHEMA_VERSION};
    std::string hypothesis_id;
    std::string assessment_id;
    std::string prompt;
    double expected_information_gain{0.0};
    std::string created_at;
    QuestionStatus status{QuestionStatus::proposed};
    std::optional<std::string> suppression_reason;
    std::string budget_window_started_at;
    std::optional<std::string> cooldown_until;
    int correction_count{0};
    std::string provenance_module{METACOGNITION_CREATED_BY};
    std::string policy_id{METACOGNITION_INFORMATION_GAIN_POLICY_ID};

    static std::string status_string(QuestionStatus value) {
        switch (value) {
        case QuestionStatus::proposed: return "proposed";
        case QuestionStatus::suppressed: return "suppressed";
        case QuestionStatus::asked: return "asked";
        case QuestionStatus::answered: return "answered";
        }
        throw MetacognitionCuriosityError("unsupported question status");
    }

    std::string to_json() const {
        metacognition_required_string(question_id, "question_id");
        metacognition_required_string(prompt, "prompt");
        metacognition_probability(expected_information_gain, "expected_information_gain");
        if (correction_count < 0) throw MetacognitionCuriosityError("correction_count must be non-negative");
        std::ostringstream output;
        output << "{\"assessment_id\":" << metacognition_json_string(assessment_id)
               << ",\"budget_window_started_at\":" << metacognition_json_string(budget_window_started_at)
               << ",\"cooldown_until\":" << (cooldown_until ? metacognition_json_string(*cooldown_until) : "null")
               << ",\"correction_count\":" << correction_count
               << ",\"created_at\":" << metacognition_json_string(created_at)
               << ",\"expected_information_gain\":" << metacognition_json_number(expected_information_gain)
               << ",\"hypothesis_id\":" << metacognition_json_string(hypothesis_id)
               << ",\"prompt\":" << metacognition_json_string(prompt)
               << ",\"provenance\":{\"module\":" << metacognition_json_string(provenance_module)
               << ",\"policy_id\":" << metacognition_json_string(policy_id) << "}"
               << ",\"question_id\":" << metacognition_json_string(question_id)
               << ",\"schema_version\":" << metacognition_json_string(schema_version)
               << ",\"status\":" << metacognition_json_string(status_string(status))
               << ",\"suppression_reason\":" << (suppression_reason ? metacognition_json_string(*suppression_reason) : "null") << '}';
        return output.str();
    }
};

struct CuriosityResponse {
    std::string response_id;
    std::string schema_version{METACOGNITION_CURIOSITY_SCHEMA_VERSION};
    std::string question_id;
    std::string received_at;
    ResponseOutcome outcome{ResponseOutcome::inconclusive};
    bool correction{false};
    std::vector<std::string> evidence_refs;
    std::string source;
    std::optional<std::string> actor_id;

    static std::string outcome_string(ResponseOutcome value) {
        switch (value) {
        case ResponseOutcome::confirmed: return "confirmed";
        case ResponseOutcome::rejected: return "rejected";
        case ResponseOutcome::inconclusive: return "inconclusive";
        }
        throw MetacognitionCuriosityError("unsupported response outcome");
    }

    std::string to_json() const {
        metacognition_required_string(response_id, "response_id");
        metacognition_required_string(question_id, "question_id");
        metacognition_required_string(source, "source");
        std::ostringstream output;
        output << "{\"correction\":" << (correction ? "true" : "false")
               << ",\"evidence_refs\":" << metacognition_json_array(evidence_refs)
               << ",\"provenance\":{\"actor_id\":" << (actor_id ? metacognition_json_string(*actor_id) : "null")
               << ",\"source\":" << metacognition_json_string(source) << "}"
               << ",\"question_id\":" << metacognition_json_string(question_id)
               << ",\"received_at\":" << metacognition_json_string(received_at)
               << ",\"response_id\":" << metacognition_json_string(response_id)
               << ",\"outcome\":" << metacognition_json_string(outcome_string(outcome))
               << ",\"schema_version\":" << metacognition_json_string(schema_version) << '}';
        return output.str();
    }
};

class MetacognitionCuriosityEngine {
public:
    explicit MetacognitionCuriosityEngine(CuriosityConfig config = {}) : config_(std::move(config)) { config_.validate(); }

    const CuriosityConfig& config() const { return config_; }

    MetacognitiveAssessment evaluate(const HypothesisRecord& hypothesis, const std::string& now) {
        hypothesis.validate();
        const auto moment = metacognition_normalize_timestamp(now);
        hypotheses_[hypothesis.hypothesis_id] = hypothesis;
        const auto evidence_total = hypothesis.supporting_refs.size() + hypothesis.opposing_refs.size();
        const std::optional<double> evidence_balance = evidence_total == 0
            ? std::nullopt
            : std::optional<double>{static_cast<double>(hypothesis.supporting_refs.size()) / static_cast<double>(evidence_total)};
        const double raw = evidence_balance ? (hypothesis.confidence + *evidence_balance) / 2.0 : hypothesis.confidence;
        const double calibrated = calibrate(raw);
        const double uncertainty = binary_entropy(calibrated);
        const std::string decision = calibrated >= config_.silence_confidence ? "silence" : "question";
        std::vector<std::string> reasons{"confidence.raw:" + std::string(METACOGNITION_RAW_CONFIDENCE_ID)};
        reasons.push_back(evidence_balance ? "evidence.balance_observed" : "evidence.absent");
        reasons.push_back("calibration:" + calibrator_identifier());
        reasons.push_back("decision:" + decision);
        const auto assessment_id = digest::uuid5(
            METACOGNITION_NAMESPACE,
            hypothesis.hypothesis_id + ":" + moment + ":" + std::to_string(calibration_observations_.size()) + ":" + config_.fingerprint());
        MetacognitiveAssessment assessment{
            assessment_id, METACOGNITION_CURIOSITY_SCHEMA_VERSION, hypothesis.hypothesis_id, moment,
            raw, calibrated, evidence_balance, uncertainty, hypothesis.supporting_refs, hypothesis.opposing_refs,
            hypothesis.alternatives, decision, std::move(reasons), calibrator_identifier()};
        assessment.to_json();
        assessments_[assessment_id] = assessment;
        return assessment;
    }

    CuriosityQuestion propose_question(const std::string& assessment_id, const std::string& prompt,
                                       double expected_resolution, const std::string& now) {
        const auto assessment = find_assessment(assessment_id);
        metacognition_required_string(prompt, "prompt");
        metacognition_probability(expected_resolution, "expected_resolution");
        const auto moment = metacognition_normalize_timestamp(now);
        const auto fingerprint = metacognition_question_fingerprint(prompt);
        const double expected_gain = expected_gain_for(assessment, expected_resolution);
        const auto& hypothesis_id = assessment.hypothesis_id;
        const int correction_count = correction_count_.contains(hypothesis_id) ? correction_count_.at(hypothesis_id) : 0;
        const auto suppression = suppression_reason(assessment, hypothesis_id, fingerprint, expected_gain, moment);
        const auto question_id = digest::uuid5(
            METACOGNITION_NAMESPACE,
            assessment_id + ":" + fingerprint + ":" + moment + ":" + std::to_string(questions_.size()));
        std::optional<std::string> cooldown;
        if (const auto found = cooldown_until_.find(hypothesis_id); found != cooldown_until_.end()) cooldown = found->second;
        CuriosityQuestion question{
            question_id, METACOGNITION_CURIOSITY_SCHEMA_VERSION, hypothesis_id, assessment_id, prompt, expected_gain,
            moment, suppression ? QuestionStatus::suppressed : QuestionStatus::proposed, suppression,
            metacognition_format_timestamp(metacognition_parse_timestamp(moment) - config_.interruption_window_seconds),
            cooldown, correction_count, METACOGNITION_CREATED_BY, config_.policy_string()};
        question.to_json();
        questions_[question_id] = question;
        return question;
    }

    CuriosityQuestion ask(const std::string& question_id, const std::string& now) {
        const auto found = questions_.find(question_id);
        if (found == questions_.end()) throw MetacognitionCuriosityError("question is unavailable");
        if (found->second.status != QuestionStatus::proposed) throw MetacognitionCuriosityError("only proposed questions can be asked");
        const auto moment = metacognition_normalize_timestamp(now);
        if (config_.budget_enabled && !budget_available(moment)) throw MetacognitionCuriosityError("interruption budget is exhausted");
        auto updated = found->second;
        updated.status = QuestionStatus::asked;
        updated.to_json();
        found->second = updated;
        asked_at_.push_back(moment);
        asked_fingerprints_.insert({updated.hypothesis_id, metacognition_question_fingerprint(updated.prompt)});
        if (config_.cooldown_enabled) cooldown_until_[updated.hypothesis_id] = metacognition_format_timestamp(metacognition_parse_timestamp(moment) + config_.cooldown_seconds);
        return updated;
    }

    CuriosityResponse record_response(const std::string& question_id, ResponseOutcome outcome, bool correction,
                                      std::vector<std::string> evidence_refs, const std::string& source,
                                      std::optional<std::string> actor_id, const std::string& now) {
        const auto found = questions_.find(question_id);
        if (found == questions_.end()) throw MetacognitionCuriosityError("question is unavailable");
        if (found->second.status != QuestionStatus::asked) throw MetacognitionCuriosityError("only asked questions can receive a response");
        metacognition_required_string(source, "source");
        if (actor_id) metacognition_required_string(*actor_id, "actor_id");
        for (const auto& reference : evidence_refs) if (reference.empty()) throw MetacognitionCuriosityError("evidence_refs must contain non-empty strings");
        const auto moment = metacognition_normalize_timestamp(now);
        const auto outcome_value = CuriosityResponse::outcome_string(outcome);
        const auto response_id = digest::uuid5(
            METACOGNITION_NAMESPACE,
            question_id + ":" + moment + ":" + outcome_value + ":" + metacognition_tuple_repr(evidence_refs));
        CuriosityResponse response{response_id, METACOGNITION_CURIOSITY_SCHEMA_VERSION, question_id, moment, outcome,
                                   correction, std::move(evidence_refs), source, std::move(actor_id)};
        response.to_json();
        auto updated = found->second;
        updated.status = QuestionStatus::answered;
        found->second = updated;
        responses_.push_back(response);
        const auto assessment = find_assessment(found->second.assessment_id);
        if (outcome != ResponseOutcome::inconclusive) {
            const double verified = outcome == ResponseOutcome::confirmed ? 1.0 : 0.0;
            calibration_observations_.push_back({assessment.raw_confidence, verified});
            metric_outcomes_.push_back({assessment.calibrated_confidence, verified});
        }
        if (correction) {
            const int count = (correction_count_.contains(found->second.hypothesis_id) ? correction_count_.at(found->second.hypothesis_id) : 0) + 1;
            correction_count_[found->second.hypothesis_id] = count;
            if (config_.cooldown_enabled) cooldown_until_[found->second.hypothesis_id] = metacognition_format_timestamp(metacognition_parse_timestamp(moment) + config_.correction_cooldown_seconds);
        }
        return response;
    }

    std::string metrics_json() const {
        const auto calibration = calibration_metrics(metric_outcomes_);
        std::ostringstream output;
        output << "{\"ablation\":" << metacognition_json_string(METACOGNITION_ABLATION)
               << ",\"baseline_confidence_id\":" << metacognition_json_string(METACOGNITION_BASELINE_CONFIDENCE_ID)
               << ",\"baseline_question_policy_id\":" << metacognition_json_string(METACOGNITION_BASELINE_QUESTION_POLICY_ID)
               << ",\"calibration\":" << calibration
               << ",\"confidence_policy_id\":" << metacognition_json_string(calibrator_identifier())
               << ",\"falsification\":" << metacognition_json_string(METACOGNITION_FALSIFICATION)
               << ",\"hypothesis\":" << metacognition_json_string(METACOGNITION_HYPOTHESIS)
               << ",\"question_policy_id\":" << metacognition_json_string(config_.policy_string())
               << ",\"questions\":[";
        bool first = true;
        for (const auto& [unused, question] : questions_) {
            if (!first) output << ',';
            first = false;
            output << "{\"expected_information_gain\":" << metacognition_json_number(question.expected_information_gain)
                   << ",\"question_id\":" << metacognition_json_string(question.question_id)
                   << ",\"status\":" << metacognition_json_string(CuriosityQuestion::status_string(question.status))
                   << ",\"suppression_reason\":" << (question.suppression_reason ? metacognition_json_string(*question.suppression_reason) : "null") << '}';
        }
        output << "],\"registered\":true,\"responses\":[";
        for (std::size_t index = 0; index < responses_.size(); ++index) {
            if (index) output << ',';
            output << responses_[index].to_json();
        }
        output << "]}";
        return output.str();
    }

    std::string snapshot_json() const {
        std::ostringstream output;
        output << "{\"assessments\":[";
        bool first = true;
        for (const auto& [unused, assessment] : assessments_) {
            if (!first) output << ',';
            first = false;
            output << assessment.to_json();
        }
        output << "],\"config_fingerprint\":" << metacognition_json_string(config_.fingerprint())
               << ",\"hypotheses\":[";
        first = true;
        for (const auto& [unused, hypothesis] : hypotheses_) {
            if (!first) output << ',';
            first = false;
            output << hypothesis.to_json();
        }
        output << "],\"metrics\":" << metrics_json() << ",\"questions\":[";
        first = true;
        for (const auto& [unused, question] : questions_) {
            if (!first) output << ',';
            first = false;
            output << question.to_json();
        }
        output << "],\"responses\":[";
        for (std::size_t index = 0; index < responses_.size(); ++index) {
            if (index) output << ',';
            output << responses_[index].to_json();
        }
        output << "],\"schema_version\":\"1.0\"}";
        return output.str();
    }

private:
    const MetacognitiveAssessment& find_assessment(const std::string& assessment_id) const {
        const auto found = assessments_.find(assessment_id);
        if (found == assessments_.end()) throw MetacognitionCuriosityError("assessment is unavailable");
        return found->second;
    }

    std::string calibrator_identifier() const {
        return config_.calibration_enabled ? METACOGNITION_CALIBRATOR_ID : METACOGNITION_BASELINE_CONFIDENCE_ID;
    }

    static int bucket(double confidence, int count) {
        return std::min(count - 1, static_cast<int>(confidence * static_cast<double>(count)));
    }

    double calibrate(double raw_confidence) const {
        if (!config_.calibration_enabled) return raw_confidence;
        const int wanted_bucket = bucket(raw_confidence, config_.calibration_bucket_count);
        std::vector<double> outcomes;
        for (const auto& [confidence, outcome] : calibration_observations_) {
            if (bucket(confidence, config_.calibration_bucket_count) == wanted_bucket) outcomes.push_back(outcome);
        }
        if (outcomes.empty()) return raw_confidence;
        double sum = 0.0;
        for (const auto outcome : outcomes) sum += outcome;
        return (2.0 * raw_confidence + sum) / (2.0 + static_cast<double>(outcomes.size()));
    }

    static double binary_entropy(double confidence) {
        if (confidence <= 0.0 || confidence >= 1.0) return 0.0;
        return -(confidence * std::log2(confidence) + (1.0 - confidence) * std::log2(1.0 - confidence));
    }

    double expected_gain_for(const MetacognitiveAssessment& assessment, double expected_resolution) const {
        if (config_.question_policy == QuestionPolicy::fixed_gain_v0) return 0.5;
        const int penalty = 1 + (correction_count_.contains(assessment.hypothesis_id) ? correction_count_.at(assessment.hypothesis_id) : 0);
        return binary_entropy(assessment.calibrated_confidence) * expected_resolution / static_cast<double>(penalty);
    }

    bool budget_available(const std::string& moment) {
        const auto epoch = metacognition_parse_timestamp(moment);
        const auto start = epoch - config_.interruption_window_seconds;
        asked_at_.erase(std::remove_if(asked_at_.begin(), asked_at_.end(), [&](const auto& value) {
            return metacognition_parse_timestamp(value) < start;
        }), asked_at_.end());
        return asked_at_.size() < static_cast<std::size_t>(config_.interruptions_per_window);
    }

    std::optional<std::string> suppression_reason(const MetacognitiveAssessment& assessment,
                                                  const std::string& hypothesis_id,
                                                  const std::string& fingerprint,
                                                  double expected_gain,
                                                  const std::string& moment) const {
        if (config_.redundancy_suppression_enabled && asked_fingerprints_.contains({hypothesis_id, fingerprint})) return "redundant_question";
        const auto cooldown = cooldown_until_.find(hypothesis_id);
        if (config_.cooldown_enabled && cooldown != cooldown_until_.end() && metacognition_parse_timestamp(moment) < metacognition_parse_timestamp(cooldown->second)) {
            return correction_count_.contains(hypothesis_id) && correction_count_.at(hypothesis_id) ? "correction_cooldown" : "cooldown";
        }
        if (config_.budget_enabled && !budget_available_const(moment)) return "interruption_budget";
        if (assessment.decision == "silence") return "sufficiently_calibrated";
        if (expected_gain < config_.min_information_gain) return "low_information_gain";
        return std::nullopt;
    }

    bool budget_available_const(const std::string& moment) const {
        const auto start = metacognition_parse_timestamp(moment) - config_.interruption_window_seconds;
        const auto active = std::count_if(asked_at_.begin(), asked_at_.end(), [&](const auto& value) {
            return metacognition_parse_timestamp(value) >= start;
        });
        return active < config_.interruptions_per_window;
    }

    std::string calibration_metrics(const std::vector<std::pair<double, double>>& outcomes) const {
        if (outcomes.empty()) return "{\"auroc\":null,\"brier\":null,\"ece\":null,\"outcome_count\":0,\"risk_coverage\":[]}";
        double brier = 0.0;
        for (const auto& [confidence, outcome] : outcomes) brier += (confidence - outcome) * (confidence - outcome);
        brier /= static_cast<double>(outcomes.size());
        std::map<int, std::vector<std::pair<double, double>>> grouped;
        for (const auto& value : outcomes) grouped[bucket(value.first, config_.calibration_bucket_count)].push_back(value);
        double ece = 0.0;
        for (const auto& [unused, values] : grouped) {
            double confidence = 0.0;
            double outcome = 0.0;
            for (const auto& [item_confidence, item_outcome] : values) { confidence += item_confidence; outcome += item_outcome; }
            confidence /= static_cast<double>(values.size());
            outcome /= static_cast<double>(values.size());
            ece += static_cast<double>(values.size()) / static_cast<double>(outcomes.size()) * std::abs(confidence - outcome);
        }
        std::vector<std::pair<double, double>> ordered = outcomes;
        std::stable_sort(ordered.begin(), ordered.end(), [](const auto& left, const auto& right) { return left.first > right.first; });
        std::ostringstream risk;
        risk << '[';
        double correct = 0.0;
        for (std::size_t index = 0; index < ordered.size(); ++index) {
            if (index) risk << ',';
            correct += ordered[index].second;
            const auto count = static_cast<double>(index + 1);
            risk << "{\"coverage\":" << metacognition_json_number(count / static_cast<double>(ordered.size()))
                 << ",\"risk\":" << metacognition_json_number(1.0 - correct / count) << '}';
        }
        risk << ']';
        std::optional<double> auroc;
        std::vector<double> positive;
        std::vector<double> negative;
        for (const auto& [confidence, outcome] : outcomes) (outcome == 1.0 ? positive : negative).push_back(confidence);
        if (!positive.empty() && !negative.empty()) {
            double comparisons = 0.0;
            for (const auto first : positive) for (const auto second : negative) comparisons += first > second ? 1.0 : first == second ? 0.5 : 0.0;
            auroc = comparisons / static_cast<double>(positive.size() * negative.size());
        }
        std::ostringstream output;
        output << "{\"auroc\":" << (auroc ? metacognition_json_number(*auroc) : "null")
               << ",\"brier\":" << metacognition_json_number(brier)
               << ",\"ece\":" << metacognition_json_number(ece)
               << ",\"outcome_count\":" << outcomes.size() << ",\"risk_coverage\":" << risk.str() << '}';
        return output.str();
    }

    CuriosityConfig config_;
    std::map<std::string, HypothesisRecord> hypotheses_;
    std::map<std::string, MetacognitiveAssessment> assessments_;
    std::map<std::string, CuriosityQuestion> questions_;
    std::vector<CuriosityResponse> responses_;
    std::vector<std::pair<double, double>> calibration_observations_;
    std::vector<std::pair<double, double>> metric_outcomes_;
    std::vector<std::string> asked_at_;
    std::set<std::pair<std::string, std::string>> asked_fingerprints_;
    std::map<std::string, std::string> cooldown_until_;
    std::map<std::string, int> correction_count_;
};

class MetacognitionCuriosityPlugin final : public CapabilityPlugin {
public:
    MetacognitionCuriosityPlugin() {
        descriptor_.capability_id = "cognition.metacognition_curiosity";
        descriptor_.implementation_id = "native.metacognition_curiosity";
        descriptor_.implementation_version = "1.0.0";
        descriptor_.kind = "cognitive_service";
        descriptor_.provides.push_back({"propose.curiosity", "urn:eu-digital:curiosity-question:1"});
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
