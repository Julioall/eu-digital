#pragma once

#include <cctype>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace eu_digital::ollama_json {

class Error final : public std::runtime_error {
public:
  explicit Error(const std::string &message) : std::runtime_error(message) {}
};

struct JsonNumber {
  std::string text;
};

struct JsonValue {
  using Array = std::vector<JsonValue>;
  using Object = std::map<std::string, JsonValue>;
  std::variant<std::nullptr_t, bool, JsonNumber, std::string, Array, Object>
      value;
};

class JsonParser {
public:
  explicit JsonParser(std::string_view text) : text_(text) {}

  JsonValue parse() {
    validate_utf8(text_);
    auto result = parse_value();
    skip_space();
    if (position_ != text_.size())
      fail("unexpected trailing JSON input");
    return result;
  }

private:
  JsonValue parse_value() {
    skip_space();
    if (position_ == text_.size())
      fail("unexpected end of JSON input");
    switch (text_[position_]) {
    case '{':
      return parse_object();
    case '[':
      return parse_array();
    case '"':
      return JsonValue{parse_string()};
    case 't':
      consume_literal("true");
      return JsonValue{true};
    case 'f':
      consume_literal("false");
      return JsonValue{false};
    case 'n':
      consume_literal("null");
      return JsonValue{nullptr};
    default:
      return JsonValue{JsonNumber{parse_number()}};
    }
  }

  JsonValue parse_object() {
    expect('{');
    JsonValue::Object object;
    skip_space();
    if (consume('}'))
      return JsonValue{std::move(object)};
    while (true) {
      skip_space();
      if (position_ == text_.size() || text_[position_] != '"') {
        fail("object key must be a string");
      }
      auto key = parse_string();
      if (object.contains(key))
        fail("duplicate JSON object key");
      skip_space();
      expect(':');
      object.emplace(std::move(key), parse_value());
      skip_space();
      if (consume('}'))
        return JsonValue{std::move(object)};
      expect(',');
    }
  }

  JsonValue parse_array() {
    expect('[');
    JsonValue::Array array;
    skip_space();
    if (consume(']'))
      return JsonValue{std::move(array)};
    while (true) {
      array.push_back(parse_value());
      skip_space();
      if (consume(']'))
        return JsonValue{std::move(array)};
      expect(',');
    }
  }

  std::string parse_string() {
    expect('"');
    std::string result;
    while (position_ < text_.size()) {
      const char character = text_[position_++];
      if (character == '"')
        return result;
      if (static_cast<unsigned char>(character) < 0x20) {
        fail("control character in JSON string");
      }
      if (character != '\\') {
        result.push_back(character);
        continue;
      }
      if (position_ == text_.size())
        fail("unterminated JSON escape");
      const char escaped = text_[position_++];
      switch (escaped) {
      case '"':
        result.push_back('"');
        break;
      case '\\':
        result.push_back('\\');
        break;
      case '/':
        result.push_back('/');
        break;
      case 'b':
        result.push_back('\b');
        break;
      case 'f':
        result.push_back('\f');
        break;
      case 'n':
        result.push_back('\n');
        break;
      case 'r':
        result.push_back('\r');
        break;
      case 't':
        result.push_back('\t');
        break;
      case 'u':
        append_unicode_escape(result);
        break;
      default:
        fail("unsupported JSON escape");
      }
    }
    fail("unterminated JSON string");
  }

  std::string parse_number() {
    const auto begin = position_;
    consume('-');
    if (position_ == text_.size())
      fail("invalid JSON number");
    if (text_[position_] == '0') {
      ++position_;
    } else {
      if (!std::isdigit(static_cast<unsigned char>(text_[position_]))) {
        fail("invalid JSON number");
      }
      while (position_ < text_.size() &&
             std::isdigit(static_cast<unsigned char>(text_[position_]))) {
        ++position_;
      }
    }
    if (consume('.')) {
      if (position_ == text_.size() ||
          !std::isdigit(static_cast<unsigned char>(text_[position_]))) {
        fail("invalid JSON fraction");
      }
      while (position_ < text_.size() &&
             std::isdigit(static_cast<unsigned char>(text_[position_]))) {
        ++position_;
      }
    }
    if (position_ < text_.size() &&
        (text_[position_] == 'e' || text_[position_] == 'E')) {
      ++position_;
      if (position_ < text_.size() &&
          (text_[position_] == '+' || text_[position_] == '-')) {
        ++position_;
      }
      if (position_ == text_.size() ||
          !std::isdigit(static_cast<unsigned char>(text_[position_]))) {
        fail("invalid JSON exponent");
      }
      while (position_ < text_.size() &&
             std::isdigit(static_cast<unsigned char>(text_[position_]))) {
        ++position_;
      }
    }
    return std::string(text_.substr(begin, position_ - begin));
  }

  void consume_literal(std::string_view literal) {
    if (text_.substr(position_, literal.size()) != literal) {
      fail("invalid JSON literal");
    }
    position_ += literal.size();
  }

  std::uint32_t parse_hex_codepoint() {
    if (position_ + 4 > text_.size())
      fail("short unicode escape");
    std::uint32_t codepoint = 0;
    for (int index = 0; index < 4; ++index) {
      const char character = text_[position_++];
      codepoint <<= 4;
      if (character >= '0' && character <= '9') {
        codepoint |= static_cast<std::uint32_t>(character - '0');
      } else if (character >= 'a' && character <= 'f') {
        codepoint |= static_cast<std::uint32_t>(character - 'a' + 10);
      } else if (character >= 'A' && character <= 'F') {
        codepoint |= static_cast<std::uint32_t>(character - 'A' + 10);
      } else {
        fail("invalid unicode escape");
      }
    }
    return codepoint;
  }

  void append_unicode_escape(std::string &output) {
    auto codepoint = parse_hex_codepoint();
    if (codepoint >= 0xd800 && codepoint <= 0xdbff) {
      if (position_ + 2 > text_.size() || text_[position_] != '\\' ||
          text_[position_ + 1] != 'u') {
        fail("high surrogate without low surrogate");
      }
      position_ += 2;
      const auto low = parse_hex_codepoint();
      if (low < 0xdc00 || low > 0xdfff)
        fail("invalid low surrogate");
      codepoint = 0x10000 + ((codepoint - 0xd800) << 10) + (low - 0xdc00);
    } else if (codepoint >= 0xdc00 && codepoint <= 0xdfff) {
      fail("unexpected low surrogate");
    }
    append_utf8(output, codepoint);
  }

  static void append_utf8(std::string &output, std::uint32_t codepoint) {
    if (codepoint <= 0x7f) {
      output.push_back(static_cast<char>(codepoint));
    } else if (codepoint <= 0x7ff) {
      output.push_back(static_cast<char>(0xc0 | (codepoint >> 6)));
      output.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
    } else if (codepoint <= 0xffff) {
      output.push_back(static_cast<char>(0xe0 | (codepoint >> 12)));
      output.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)));
      output.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
    } else {
      output.push_back(static_cast<char>(0xf0 | (codepoint >> 18)));
      output.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3f)));
      output.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)));
      output.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
    }
  }

  static void validate_utf8(std::string_view text) {
    for (std::size_t index = 0; index < text.size();) {
      const auto first = static_cast<unsigned char>(text[index]);
      if (first <= 0x7f) {
        ++index;
        continue;
      }
      std::size_t count = 0;
      std::uint32_t codepoint = 0;
      if (first >= 0xc2 && first <= 0xdf) {
        count = 2;
        codepoint = first & 0x1f;
      } else if (first >= 0xe0 && first <= 0xef) {
        count = 3;
        codepoint = first & 0x0f;
      } else if (first >= 0xf0 && first <= 0xf4) {
        count = 4;
        codepoint = first & 0x07;
      } else {
        fail("invalid UTF-8 in JSON input");
      }
      if (index + count > text.size())
        fail("truncated UTF-8 in JSON input");
      for (std::size_t offset = 1; offset < count; ++offset) {
        const auto continuation =
            static_cast<unsigned char>(text[index + offset]);
        if ((continuation & 0xc0) != 0x80) {
          fail("invalid UTF-8 continuation in JSON input");
        }
        codepoint = (codepoint << 6) | (continuation & 0x3f);
      }
      if ((count == 3 && codepoint < 0x800) ||
          (count == 4 && codepoint < 0x10000) ||
          (codepoint >= 0xd800 && codepoint <= 0xdfff) ||
          codepoint > 0x10ffff) {
        fail("invalid UTF-8 codepoint in JSON input");
      }
      index += count;
    }
  }

  void skip_space() {
    while (position_ < text_.size() &&
           std::isspace(static_cast<unsigned char>(text_[position_]))) {
      ++position_;
    }
  }

  bool consume(char expected) {
    if (position_ < text_.size() && text_[position_] == expected) {
      ++position_;
      return true;
    }
    return false;
  }

  void expect(char expected) {
    skip_space();
    if (!consume(expected)) {
      fail(std::string("expected JSON character: ") + expected);
    }
  }

  [[noreturn]] static void fail(const std::string &message) {
    throw Error(message);
  }

  std::string_view text_;
  std::size_t position_{0};
};

inline const JsonValue::Object &object(const JsonValue &value,
                                       const std::string &path) {
  if (!std::holds_alternative<JsonValue::Object>(value.value)) {
    throw Error(path + " must be an object");
  }
  return std::get<JsonValue::Object>(value.value);
}

inline const JsonValue::Array &array(const JsonValue &value,
                                     const std::string &path) {
  if (!std::holds_alternative<JsonValue::Array>(value.value)) {
    throw Error(path + " must be an array");
  }
  return std::get<JsonValue::Array>(value.value);
}

inline const JsonValue &required(const JsonValue::Object &object_value,
                                 const std::string &key,
                                 const std::string &path) {
  const auto found = object_value.find(key);
  if (found == object_value.end()) {
    throw Error(path + " missing required field: " + key);
  }
  return found->second;
}

inline std::string string(const JsonValue &value, const std::string &path) {
  if (!std::holds_alternative<std::string>(value.value)) {
    throw Error(path + " must be a string");
  }
  const auto &result = std::get<std::string>(value.value);
  if (result.empty())
    throw Error(path + " must not be empty");
  return result;
}

inline bool boolean(const JsonValue &value, const std::string &path) {
  if (!std::holds_alternative<bool>(value.value)) {
    throw Error(path + " must be a boolean");
  }
  return std::get<bool>(value.value);
}

inline std::uint64_t unsigned_number(const JsonValue &value,
                                     const std::string &path) {
  if (!std::holds_alternative<JsonNumber>(value.value)) {
    throw Error(path + " must be a non-negative integer");
  }
  const auto &text = std::get<JsonNumber>(value.value).text;
  if (text.empty() || text[0] == '-' ||
      text.find_first_not_of("0123456789") != std::string::npos) {
    throw Error(path + " must be a non-negative integer");
  }
  std::uint64_t result = 0;
  for (const char character : text) {
    const auto digit = static_cast<std::uint64_t>(character - '0');
    if (result > (std::numeric_limits<std::uint64_t>::max() - digit) / 10) {
      throw Error(path + " is too large");
    }
    result = result * 10 + digit;
  }
  return result;
}

inline void exact_keys(const JsonValue::Object &object_value,
                       std::initializer_list<const char *> keys,
                       const std::string &path) {
  if (object_value.size() != keys.size()) {
    throw Error(path + " has unknown or missing fields");
  }
  for (const auto *key : keys)
    required(object_value, key, path);
}

} // namespace eu_digital::ollama_json
