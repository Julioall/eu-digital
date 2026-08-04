#pragma once

#include "core/digest.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace eu_digital::contracts {

inline constexpr char kCognitiveOutputSchemaVersion[] = "1.0";
inline constexpr char kCognitiveOutputNamespace[] =
    "3d76379c-e8f1-4ef4-99ae-b51d8315be51";

enum class CognitiveOutputIntentV1 {
    silence,
    question,
    requested_response,
    proactive_suggestion,
};

enum class DialogueOutputStatusV1 {
    rendered,
    malformed,
    timeout,
    fallback_used,
    silence,
    unavailable,
};

inline std::string cognitive_output_intent_string(CognitiveOutputIntentV1 value) {
    switch (value) {
    case CognitiveOutputIntentV1::silence: return "silence";
    case CognitiveOutputIntentV1::question: return "question";
    case CognitiveOutputIntentV1::requested_response: return "requested_response";
    case CognitiveOutputIntentV1::proactive_suggestion: return "proactive_suggestion";
    }
    return "silence";
}

inline std::optional<CognitiveOutputIntentV1> cognitive_output_intent(
    std::string_view value) {
    if (value == "silence") return CognitiveOutputIntentV1::silence;
    if (value == "question") return CognitiveOutputIntentV1::question;
    if (value == "requested_response") {
        return CognitiveOutputIntentV1::requested_response;
    }
    if (value == "proactive_suggestion") {
        return CognitiveOutputIntentV1::proactive_suggestion;
    }
    return std::nullopt;
}

inline std::string dialogue_output_status_string(DialogueOutputStatusV1 value) {
    switch (value) {
    case DialogueOutputStatusV1::rendered: return "rendered";
    case DialogueOutputStatusV1::malformed: return "malformed";
    case DialogueOutputStatusV1::timeout: return "timeout";
    case DialogueOutputStatusV1::fallback_used: return "fallback_used";
    case DialogueOutputStatusV1::silence: return "silence";
    case DialogueOutputStatusV1::unavailable: return "unavailable";
    }
    return "unavailable";
}

namespace cognitive_output_detail {

inline bool bounded_non_empty(const std::string& value, std::size_t maximum) {
    return !value.empty() && value.size() <= maximum;
}

inline bool unique_bounded(const std::vector<std::string>& values,
                           std::size_t maximum_count,
                           std::size_t maximum_length,
                           bool allow_empty) {
    if ((!allow_empty && values.empty()) || values.size() > maximum_count) {
        return false;
    }
    std::set<std::string> unique;
    for (const auto& value : values) {
        if (!bounded_non_empty(value, maximum_length) || !unique.insert(value).second) {
            return false;
        }
    }
    return true;
}

inline std::string json_string(const std::string& value) {
    std::ostringstream output;
    output << '"';
    constexpr char digits[] = "0123456789abcdef";
    for (const unsigned char character : value) {
        switch (character) {
        case '"': output << "\\\""; break;
        case '\\': output << "\\\\"; break;
        case '\b': output << "\\b"; break;
        case '\f': output << "\\f"; break;
        case '\n': output << "\\n"; break;
        case '\r': output << "\\r"; break;
        case '\t': output << "\\t"; break;
        default:
            if (character < 0x20U) {
                output << "\\u00" << digits[(character >> 4U) & 0x0fU]
                       << digits[character & 0x0fU];
            } else {
                output << static_cast<char>(character);
            }
        }
    }
    output << '"';
    return output.str();
}

inline std::string json_array(const std::vector<std::string>& values) {
    std::ostringstream output;
    output << '[';
    for (std::size_t index = 0; index < values.size(); ++index) {
        if (index) output << ',';
        output << json_string(values[index]);
    }
    output << ']';
    return output.str();
}

inline std::string json_map(const std::map<std::string, std::string>& values) {
    std::ostringstream output;
    output << '{';
    std::size_t index = 0;
    for (const auto& [key, value] : values) {
        if (index++) output << ',';
        output << json_string(key) << ':' << json_string(value);
    }
    output << '}';
    return output.str();
}

inline void append_unique(std::vector<std::string>& values,
                          const std::string& value) {
    if (!value.empty() &&
        std::find(values.begin(), values.end(), value) == values.end()) {
        values.push_back(value);
    }
}

class CandidateParser {
public:
    explicit CandidateParser(std::string_view input) : input_(input) {}

    struct Value {
        std::string schema_version;
        std::string request_id;
        std::string intent;
        std::string rendered_text;
        std::vector<std::string> evidence_refs;
    };

    Value parse() {
        Value result;
        std::set<std::string> keys;
        expect('{');
        skip_space();
        if (consume('}')) fail("candidate cannot be empty");
        while (true) {
            const auto key = parse_string();
            if (!keys.insert(key).second) fail("duplicate candidate field");
            skip_space();
            expect(':');
            if (key == "schema_version") result.schema_version = parse_string();
            else if (key == "request_id") result.request_id = parse_string();
            else if (key == "intent") result.intent = parse_string();
            else if (key == "rendered_text") result.rendered_text = parse_string();
            else if (key == "evidence_refs") result.evidence_refs = parse_string_array();
            else fail("unknown candidate field");
            skip_space();
            if (consume('}')) break;
            expect(',');
        }
        skip_space();
        if (position_ != input_.size()) fail("trailing candidate content");
        static const std::set<std::string> required{
            "schema_version", "request_id", "intent", "rendered_text",
            "evidence_refs"};
        if (keys != required) fail("candidate fields do not match schema 1.0");
        return result;
    }

private:
    std::vector<std::string> parse_string_array() {
        std::vector<std::string> result;
        expect('[');
        skip_space();
        if (consume(']')) return result;
        while (true) {
            result.push_back(parse_string());
            skip_space();
            if (consume(']')) return result;
            expect(',');
        }
    }

    std::string parse_string() {
        skip_space();
        expect('"');
        std::string result;
        while (position_ < input_.size()) {
            const unsigned char character =
                static_cast<unsigned char>(input_[position_++]);
            if (character == '"') return result;
            if (character < 0x20U) fail("control character in candidate string");
            if (character != '\\') {
                result.push_back(static_cast<char>(character));
                continue;
            }
            if (position_ == input_.size()) fail("unterminated candidate escape");
            switch (input_[position_++]) {
            case '"': result.push_back('"'); break;
            case '\\': result.push_back('\\'); break;
            case '/': result.push_back('/'); break;
            case 'b': result.push_back('\b'); break;
            case 'f': result.push_back('\f'); break;
            case 'n': result.push_back('\n'); break;
            case 'r': result.push_back('\r'); break;
            case 't': result.push_back('\t'); break;
            case 'u': append_utf8(result, parse_codepoint()); break;
            default: fail("unsupported candidate escape");
            }
        }
        fail("unterminated candidate string");
    }

    std::uint32_t parse_codepoint() {
        if (position_ + 4 > input_.size()) fail("short unicode escape");
        std::uint32_t result = 0;
        for (int index = 0; index < 4; ++index) {
            result <<= 4U;
            const char character = input_[position_++];
            if (character >= '0' && character <= '9') {
                result |= static_cast<std::uint32_t>(character - '0');
            } else if (character >= 'a' && character <= 'f') {
                result |= static_cast<std::uint32_t>(character - 'a' + 10);
            } else if (character >= 'A' && character <= 'F') {
                result |= static_cast<std::uint32_t>(character - 'A' + 10);
            } else {
                fail("invalid unicode escape");
            }
        }
        if (result >= 0xd800U && result <= 0xdfffU) {
            fail("surrogate unicode escape is unsupported");
        }
        return result;
    }

    static void append_utf8(std::string& output, std::uint32_t codepoint) {
        if (codepoint <= 0x7fU) {
            output.push_back(static_cast<char>(codepoint));
        } else if (codepoint <= 0x7ffU) {
            output.push_back(static_cast<char>(0xc0U | (codepoint >> 6U)));
            output.push_back(static_cast<char>(0x80U | (codepoint & 0x3fU)));
        } else {
            output.push_back(static_cast<char>(0xe0U | (codepoint >> 12U)));
            output.push_back(static_cast<char>(0x80U | ((codepoint >> 6U) & 0x3fU)));
            output.push_back(static_cast<char>(0x80U | (codepoint & 0x3fU)));
        }
    }

    void skip_space() {
        while (position_ < input_.size() &&
               std::isspace(static_cast<unsigned char>(input_[position_]))) {
            ++position_;
        }
    }

    bool consume(char character) {
        skip_space();
        if (position_ < input_.size() && input_[position_] == character) {
            ++position_;
            return true;
        }
        return false;
    }

    void expect(char character) {
        if (!consume(character)) fail("unexpected candidate token");
    }

    [[noreturn]] static void fail(const char* message) {
        throw std::invalid_argument(message);
    }

    std::string_view input_;
    std::size_t position_{0};
};

}  // namespace cognitive_output_detail

struct CognitiveOutputRequestV1 {
    std::string schema_version{kCognitiveOutputSchemaVersion};
    std::string request_id;
    std::string correlation_id;
    std::string input_event_id;
    CognitiveOutputIntentV1 intent{CognitiveOutputIntentV1::silence};
    std::string occurred_at;
    bool critical{false};
    std::string reason;
    std::map<std::string, std::string> input_content;
    std::optional<std::string> self_model_id;
    std::vector<std::string> self_constraints;
    std::vector<std::string> evidence_refs;

    bool valid() const {
        const bool expected_critical =
            intent == CognitiveOutputIntentV1::question ||
            intent == CognitiveOutputIntentV1::requested_response;
        if (schema_version != kCognitiveOutputSchemaVersion ||
            !cognitive_output_detail::bounded_non_empty(request_id, 256) ||
            !cognitive_output_detail::bounded_non_empty(correlation_id, 256) ||
            !cognitive_output_detail::bounded_non_empty(input_event_id, 256) ||
            !cognitive_output_detail::bounded_non_empty(occurred_at, 128) ||
            !cognitive_output_detail::bounded_non_empty(reason, 2048) ||
            critical != expected_critical || input_content.size() > 32 ||
            (self_model_id && !cognitive_output_detail::bounded_non_empty(
                                  *self_model_id, 256)) ||
            (!self_model_id && !self_constraints.empty()) ||
            !cognitive_output_detail::unique_bounded(
                self_constraints, 64, 1024, true) ||
            !cognitive_output_detail::unique_bounded(
                evidence_refs, 256, 256, false)) {
            return false;
        }
        for (const auto& [key, value] : input_content) {
            if (!cognitive_output_detail::bounded_non_empty(key, 128) ||
                value.size() > 8192) {
                return false;
            }
        }
        return std::find(evidence_refs.begin(), evidence_refs.end(),
                         input_event_id) != evidence_refs.end();
    }

    std::string to_json() const {
        if (!valid()) throw std::invalid_argument("invalid cognitive output request");
        std::ostringstream output;
        output << "{\"correlation_id\":"
               << cognitive_output_detail::json_string(correlation_id)
               << ",\"critical\":" << (critical ? "true" : "false")
               << ",\"evidence_refs\":"
               << cognitive_output_detail::json_array(evidence_refs)
               << ",\"input_content\":"
               << cognitive_output_detail::json_map(input_content)
               << ",\"input_event_id\":"
               << cognitive_output_detail::json_string(input_event_id)
               << ",\"intent\":" << cognitive_output_detail::json_string(
                      cognitive_output_intent_string(intent))
               << ",\"occurred_at\":"
               << cognitive_output_detail::json_string(occurred_at)
               << ",\"reason\":" << cognitive_output_detail::json_string(reason)
               << ",\"request_id\":"
               << cognitive_output_detail::json_string(request_id)
               << ",\"schema_version\":\"1.0\",\"self_constraints\":"
               << cognitive_output_detail::json_array(self_constraints)
               << ",\"self_model_id\":";
        if (self_model_id) {
            output << cognitive_output_detail::json_string(*self_model_id);
        } else {
            output << "null";
        }
        output << '}';
        return output.str();
    }
};

struct LanguageRenderingCandidateV1 {
    std::string schema_version{kCognitiveOutputSchemaVersion};
    std::string request_id;
    CognitiveOutputIntentV1 intent{CognitiveOutputIntentV1::silence};
    std::string rendered_text;
    std::vector<std::string> evidence_refs;

    bool valid_for(const CognitiveOutputRequestV1& request) const {
        if (!request.valid() || schema_version != kCognitiveOutputSchemaVersion ||
            request_id != request.request_id || intent != request.intent ||
            intent == CognitiveOutputIntentV1::silence ||
            !cognitive_output_detail::bounded_non_empty(rendered_text, 8192) ||
            !cognitive_output_detail::unique_bounded(
                evidence_refs, 256, 256, true)) {
            return false;
        }
        return std::all_of(evidence_refs.begin(), evidence_refs.end(),
                           [&](const auto& reference) {
                               return std::find(request.evidence_refs.begin(),
                                                request.evidence_refs.end(),
                                                reference) !=
                                      request.evidence_refs.end();
                           });
    }

    static LanguageRenderingCandidateV1 parse_strict(std::string_view json) {
        const auto parsed = cognitive_output_detail::CandidateParser(json).parse();
        const auto intent = cognitive_output_intent(parsed.intent);
        if (!intent) throw std::invalid_argument("unknown candidate intent");
        return {parsed.schema_version, parsed.request_id, *intent,
                parsed.rendered_text, parsed.evidence_refs};
    }
};

struct ValidatedDialogueOutputV1 {
    std::string schema_version{kCognitiveOutputSchemaVersion};
    std::string output_id;
    std::string request_id;
    CognitiveOutputIntentV1 intent{CognitiveOutputIntentV1::silence};
    DialogueOutputStatusV1 status{DialogueOutputStatusV1::unavailable};
    std::string rendered_text;
    std::vector<std::string> evidence_refs;
    std::string renderer_id;
    std::string reason_code;

    bool valid() const {
        const bool has_text = status == DialogueOutputStatusV1::rendered ||
                              status == DialogueOutputStatusV1::fallback_used;
        return schema_version == kCognitiveOutputSchemaVersion &&
               cognitive_output_detail::bounded_non_empty(output_id, 256) &&
               cognitive_output_detail::bounded_non_empty(request_id, 256) &&
               cognitive_output_detail::bounded_non_empty(renderer_id, 256) &&
               cognitive_output_detail::bounded_non_empty(reason_code, 256) &&
               (has_text
                    ? cognitive_output_detail::bounded_non_empty(rendered_text, 8192)
                    : rendered_text.empty()) &&
               cognitive_output_detail::unique_bounded(
                   evidence_refs, 256, 256, true);
    }

    bool presentable() const {
        return valid() && (status == DialogueOutputStatusV1::rendered ||
                           status == DialogueOutputStatusV1::fallback_used);
    }

    std::string to_json() const {
        if (!valid()) throw std::invalid_argument("invalid dialogue output");
        std::ostringstream output;
        output << "{\"evidence_refs\":"
               << cognitive_output_detail::json_array(evidence_refs)
               << ",\"intent\":" << cognitive_output_detail::json_string(
                      cognitive_output_intent_string(intent))
               << ",\"output_id\":"
               << cognitive_output_detail::json_string(output_id)
               << ",\"reason_code\":"
               << cognitive_output_detail::json_string(reason_code)
               << ",\"rendered_text\":"
               << cognitive_output_detail::json_string(rendered_text)
               << ",\"renderer_id\":"
               << cognitive_output_detail::json_string(renderer_id)
               << ",\"request_id\":"
               << cognitive_output_detail::json_string(request_id)
               << ",\"schema_version\":\"1.0\",\"status\":"
               << cognitive_output_detail::json_string(
                      dialogue_output_status_string(status))
               << '}';
        return output.str();
    }

    static ValidatedDialogueOutputV1 from_candidate(
        const CognitiveOutputRequestV1& request,
        const LanguageRenderingCandidateV1& candidate,
        std::string renderer_id) {
        if (!candidate.valid_for(request)) {
            throw std::invalid_argument("candidate violates cognitive output request");
        }
        return make(request, DialogueOutputStatusV1::rendered,
                    candidate.rendered_text, candidate.evidence_refs,
                    std::move(renderer_id), "candidate_validated");
    }

    static ValidatedDialogueOutputV1 fallback(
        const CognitiveOutputRequestV1& request,
        std::string renderer_id,
        std::string reason_code) {
        return make(
            request, DialogueOutputStatusV1::fallback_used,
            "Não foi possível gerar uma resposta local segura agora.", {},
            std::move(renderer_id), std::move(reason_code));
    }

    static ValidatedDialogueOutputV1 silence(
        const CognitiveOutputRequestV1& request,
        std::string renderer_id,
        std::string reason_code) {
        return make(request, DialogueOutputStatusV1::silence, "", {},
                    std::move(renderer_id), std::move(reason_code));
    }

    static ValidatedDialogueOutputV1 error(
        const CognitiveOutputRequestV1& request,
        DialogueOutputStatusV1 status,
        std::string renderer_id,
        std::string reason_code) {
        if (status == DialogueOutputStatusV1::rendered ||
            status == DialogueOutputStatusV1::fallback_used) {
            throw std::invalid_argument("error output cannot carry a text status");
        }
        return make(request, status, "", {}, std::move(renderer_id),
                    std::move(reason_code));
    }

private:
    static ValidatedDialogueOutputV1 make(
        const CognitiveOutputRequestV1& request,
        DialogueOutputStatusV1 status,
        std::string text,
        std::vector<std::string> evidence,
        std::string renderer_id,
        std::string reason_code) {
        const auto identity = request.request_id + ":" +
            dialogue_output_status_string(status) + ":" + renderer_id + ":" +
            text;
        ValidatedDialogueOutputV1 result{
            kCognitiveOutputSchemaVersion,
            digest::uuid5(kCognitiveOutputNamespace, identity),
            request.request_id,
            request.intent,
            status,
            std::move(text),
            std::move(evidence),
            std::move(renderer_id),
            std::move(reason_code)};
        if (!result.valid()) {
            throw std::invalid_argument("cannot create invalid dialogue output");
        }
        return result;
    }
};

}  // namespace eu_digital::contracts
