#include "core/runtime_host.hpp"

#include <iostream>
#include <string>

namespace {

void print_error(const std::string& code, const std::string& message) {
    std::cerr << "{\"code\":\"" << eu_digital::runtime_detail::escape_json(code)
              << "\",\"message\":\"" << eu_digital::runtime_detail::escape_json(message)
              << "\",\"fatal\":true}" << '\n';
}

void print_usage() {
    std::cerr << "usage:\n"
              << "  eu_digital_runtime <canonical-event-fixture>\n"
              << "  eu_digital_runtime --run <manifest> <timeline.sqlite> <canonical-event> <session-id> <observed-at>\n"
              << "  eu_digital_runtime --replay <manifest> <timeline.sqlite> <session-id> <observed-at>\n";
}

int run_host(const eu_digital::RuntimeConfig& config, const std::string* event_path) {
    eu_digital::RuntimeHost host(config);
    if (!host.start()) {
        std::cout << host.health_json() << '\n';
        return 1;
    }
    if (event_path != nullptr) {
        host.publish_json(eu_digital::runtime_detail::read_file(*event_path));
    } else {
        (void)host.replay();
    }
    std::cout << host.health_json() << '\n';
    const auto result = host.state() == eu_digital::RuntimeState::failed ? 1 : 0;
    host.stop();
    return result;
}

}  // namespace

int main(int argc, char** argv) {
    try {
        if (argc == 2) {
            const auto event = eu_digital::RuntimeHost::parse_canonical_event(
                eu_digital::runtime_detail::read_file(argv[1]));
            if (event.event_type != "fixture.canonical_event") {
                print_error("canonical_event_fixture_invalid", "expected event_type fixture.canonical_event");
                return 3;
            }
            std::cout << "canonical event fixture accepted\n";
            return 0;
        }
        if (argc == 7 && std::string(argv[1]) == "--run") {
            const std::string event_path = argv[4];
            return run_host({argv[2], argv[3], argv[5], argv[6]}, &event_path);
        }
        if (argc == 6 && std::string(argv[1]) == "--replay") {
            return run_host({argv[2], argv[3], argv[4], argv[5]}, nullptr);
        }
        print_usage();
        return 2;
    } catch (const std::exception& error) {
        print_error("runtime_cli_failed", error.what());
        return 1;
    }
}
