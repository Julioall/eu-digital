#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>

namespace {
std::string read_file(const std::string& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        throw std::runtime_error("cannot open fixture: " + path);
    }
    std::ostringstream contents;
    contents << stream.rdbuf();
    return contents.str();
}

bool has_string_field(const std::string& json, const std::string& field, const std::string& expected) {
    const std::regex pattern("\\\"" + field + "\\\"\\s*:\\s*\\\"([^\\\"]+)\\\"");
    std::smatch match;
    return std::regex_search(json, match, pattern) && match[1].str() == expected;
}
}  // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: eu_digital_runtime <canonical-event-fixture>\n";
        return 2;
    }
    try {
        const std::string json = read_file(argv[1]);
        if (!has_string_field(json, "schema_version", "1.0") ||
            !has_string_field(json, "event_type", "fixture.canonical_event")) {
            std::cerr << "invalid CanonicalEvent fixture\n";
            return 3;
        }
        std::cout << "canonical event fixture accepted\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
