#include "core/timeline_store.hpp"

#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

using eu_digital::CanonicalEvent;
using eu_digital::TimelineMetadata;
using eu_digital::TimelinePage;
using eu_digital::TimelineQuery;
using eu_digital::TimelineStore;

CanonicalEvent event(const char* id, const char* source, std::size_t monotonic_ns,
                     const char* payload = "{}") {
    return {.event_id = id, .source = source, .event_type = "observation",
            .payload = payload, .monotonic_ns = monotonic_ns};
}

int main() {
    const auto suffix = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto database_path = std::filesystem::temp_directory_path() /
        ("eu-digital-spec006-" + std::to_string(suffix) + ".sqlite");
    const auto export_path = std::filesystem::temp_directory_path() /
        ("eu-digital-spec006-" + std::to_string(suffix) + ".json");

    sqlite3* legacy_database = nullptr;
    assert(sqlite3_open_v2(database_path.string().c_str(), &legacy_database,
                           SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr) == SQLITE_OK);
    assert(sqlite3_exec(legacy_database, "PRAGMA user_version = 0", nullptr, nullptr, nullptr) == SQLITE_OK);
    assert(sqlite3_close(legacy_database) == SQLITE_OK);

    {
        TimelineStore store(database_path.string());
        assert(store.schema_version() == 1);
        assert(store.append(event("event-1", "shell", 20, "{\"value\":1}"),
                            TimelineMetadata{"session-a", "editor", "correlation-1"}) ==
               eu_digital::AppendResult::accepted);
        assert(store.append(event("event-2", "browser", 10),
                            TimelineMetadata{"session-a", "browser", "correlation-2"}) ==
               eu_digital::AppendResult::accepted);
        assert(store.append(event("event-3", "shell", 30),
                            TimelineMetadata{"session-b", "editor", "correlation-1"}) ==
               eu_digital::AppendResult::accepted);
        assert(store.append(event("event-1", "shell", 999, "{\"changed\":true}")) ==
               eu_digital::AppendResult::duplicate);
        assert(store.size() == 3);

        TimelineQuery temporal;
        temporal.start_monotonic_ns = 10;
        temporal.end_monotonic_ns = 31;
        const TimelinePage first_page = store.query(temporal, 2, 0);
        assert(first_page.events.size() == 2);
        assert(first_page.events[0].event.event_id == "event-2");
        assert(first_page.events[1].event.event_id == "event-1");
        assert(first_page.has_more);
        const TimelinePage second_page = store.query(temporal, 2, first_page.next_offset);
        assert(second_page.events.size() == 1);
        assert(second_page.events[0].event.event_id == "event-3");
        assert(!second_page.has_more);

        TimelineQuery by_context;
        by_context.session_id = "session-a";
        by_context.application = "editor";
        by_context.correlation_id = "correlation-1";
        const TimelinePage contextual = store.query(by_context);
        assert(contextual.events.size() == 1);
        assert(contextual.events[0].event.payload == "{\"value\":1}");

        const std::string exported = store.export_json(temporal);
        assert(exported.find("event-2") < exported.find("event-1"));
        assert(exported.find("session-a") != std::string::npos);
        store.export_json(export_path.string(), temporal);
        std::ifstream exported_file(export_path);
        const std::string exported_from_file((std::istreambuf_iterator<char>(exported_file)),
                                             std::istreambuf_iterator<char>());
        assert(exported_from_file == exported);

        const auto replayed = store.replay(temporal);
        assert(replayed.size() == 3);
        assert(replayed[0].event_id == "event-2");
        assert(replayed[1].event_id == "event-1");
        assert(replayed[2].event_id == "event-3");
        const auto replayed_again = store.replay(temporal);
        assert(replayed_again.size() == replayed.size());
        for (std::size_t index = 0; index < replayed.size(); ++index) {
            assert(replayed_again[index].event_id == replayed[index].event_id);
            assert(replayed_again[index].monotonic_ns == replayed[index].monotonic_ns);
        }

        try {
            store.append(CanonicalEvent{});
            assert(false);
        } catch (const std::invalid_argument&) {
            assert(true);
        }
    }

    {
        TimelineStore restarted(database_path.string());
        assert(restarted.schema_version() == 1);
        assert(restarted.size() == 3);
        TimelineQuery by_source;
        by_source.source = "shell";
        const TimelinePage persisted = restarted.query(by_source);
        assert(persisted.events.size() == 2);
        assert(persisted.events[0].event.event_id == "event-1");
        assert(persisted.events[1].event.event_id == "event-3");
    }

    std::filesystem::remove(database_path);
    std::filesystem::remove(export_path);
}
