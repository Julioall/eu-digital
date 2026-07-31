#pragma once

#include "event_bus.hpp"
#include "sqlite3_compat.hpp"

#include <cstdint>
#include <fstream>
#include <limits>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace eu_digital {

class TimelineStoreError : public std::runtime_error {
public:
    explicit TimelineStoreError(const std::string& message) : std::runtime_error(message) {}
};

enum class AppendResult { accepted, duplicate };

struct TimelineMetadata {
    std::string session_id;
    std::string application;
    std::string correlation_id;
};

struct TimelineRecord {
    std::uint64_t sequence{};
    CanonicalEvent event;
    TimelineMetadata metadata;
};

struct TimelineQuery {
    std::optional<std::uint64_t> start_monotonic_ns;
    std::optional<std::uint64_t> end_monotonic_ns;
    std::optional<std::string> session_id;
    std::optional<std::string> source;
    std::optional<std::string> application;
    std::optional<std::string> correlation_id;
    std::optional<std::string> event_type;
};

struct TimelinePage {
    std::vector<TimelineRecord> events;
    std::size_t next_offset{};
    bool has_more{false};
};

class TimelineStore {
public:
    explicit TimelineStore(const std::string& path) {
        sqlite3* database = nullptr;
        const int result = sqlite3_open_v2(path.c_str(), &database,
                                           SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr);
        if (result != SQLITE_OK || database == nullptr) {
            const std::string message = database == nullptr ? "cannot open SQLite database"
                                                              : sqlite_error(database, "cannot open SQLite database");
            if (database != nullptr) sqlite3_close(database);
            throw TimelineStoreError(message);
        }
        database_ = database;
        if (sqlite3_busy_timeout(database_, 5'000) != SQLITE_OK) {
            const std::string message = sqlite_error(database_, "cannot configure SQLite busy timeout");
            sqlite3_close(database_);
            database_ = nullptr;
            throw TimelineStoreError(message);
        }
        try {
            migrate();
        } catch (...) {
            sqlite3_close(database_);
            database_ = nullptr;
            throw;
        }
    }

    ~TimelineStore() {
        if (database_ != nullptr) sqlite3_close(database_);
    }

    TimelineStore(const TimelineStore&) = delete;
    TimelineStore& operator=(const TimelineStore&) = delete;

    int schema_version() const {
        Statement statement(database_, "PRAGMA user_version");
        if (statement.step() != SQLITE_ROW) throw TimelineStoreError(sqlite_error(database_, "cannot read schema version"));
        return static_cast<int>(statement.column_int64(0));
    }

    AppendResult append(const CanonicalEvent& event, const TimelineMetadata& metadata = {}) {
        if (!event.valid()) throw std::invalid_argument("invalid CanonicalEvent");
        Statement statement(database_,
            "INSERT INTO timeline_events "
            "(event_id, schema_version, source, event_type, payload, monotonic_ns, "
            "session_id, application, correlation_id) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)");
        int parameter = 1;
        statement.bind_text(parameter++, event.event_id);
        statement.bind_text(parameter++, event.schema_version);
        statement.bind_text(parameter++, event.source);
        statement.bind_text(parameter++, event.event_type);
        statement.bind_text(parameter++, event.payload);
        statement.bind_int64(parameter++, static_cast<sqlite3_int64>(event.monotonic_ns));
        statement.bind_text(parameter++, metadata.session_id);
        statement.bind_text(parameter++, metadata.application);
        statement.bind_text(parameter, metadata.correlation_id);
        const int result = statement.step();
        if (result == SQLITE_DONE) return AppendResult::accepted;
        if (result == SQLITE_CONSTRAINT) return AppendResult::duplicate;
        throw TimelineStoreError(sqlite_error(database_, "cannot append timeline event"));
    }

    std::size_t size() const {
        Statement statement(database_, "SELECT COUNT(*) FROM timeline_events");
        if (statement.step() != SQLITE_ROW) throw TimelineStoreError(sqlite_error(database_, "cannot count timeline events"));
        return static_cast<std::size_t>(statement.column_int64(0));
    }

    void save_snapshot(const std::vector<std::uint8_t>& encrypted_payload, std::int64_t created_at_ns) {
        exec("BEGIN IMMEDIATE");
        try {
            Statement statement(database_, "INSERT INTO cognitive_snapshots (created_at, payload) VALUES (?, ?)");
            statement.bind_int64(1, static_cast<sqlite3_int64>(created_at_ns));
            
            const int bind_result = sqlite3_bind_blob(statement.stmt(), 2, encrypted_payload.data(), 
                                                      static_cast<int>(encrypted_payload.size()), SQLITE_TRANSIENT);
            if (bind_result != SQLITE_OK) throw TimelineStoreError("cannot bind snapshot blob");
            
            if (statement.step() != SQLITE_DONE) throw TimelineStoreError("cannot insert snapshot");
            
            // Cleanup old snapshots, keep only the latest 2 (current and previous for fallback)
            exec("DELETE FROM cognitive_snapshots WHERE id NOT IN (SELECT id FROM cognitive_snapshots ORDER BY id DESC LIMIT 2)");
            exec("COMMIT");
        } catch (...) {
            try { exec("ROLLBACK"); } catch (...) {}
            throw;
        }
    }

    std::vector<std::vector<std::uint8_t>> load_snapshots() const {
        std::vector<std::vector<std::uint8_t>> snapshots;
        Statement statement(database_, "SELECT payload FROM cognitive_snapshots ORDER BY id DESC LIMIT 2");
        while (statement.step() == SQLITE_ROW) {
            const void* blob = sqlite3_column_blob(statement.stmt(), 0);
            const int bytes = sqlite3_column_bytes(statement.stmt(), 0);
            if (blob && bytes > 0) {
                const auto* data = static_cast<const std::uint8_t*>(blob);
                snapshots.emplace_back(data, data + bytes);
            }
        }
        return snapshots;
    }

    TimelinePage query(const TimelineQuery& query = {}, std::size_t page_size = 100,
                       std::size_t offset = 0) const {
        if (page_size == 0) throw std::invalid_argument("timeline page_size must be positive");
        if (page_size == std::numeric_limits<std::size_t>::max()) {
            throw std::invalid_argument("timeline page_size is too large");
        }

        std::string sql =
            "SELECT sequence, schema_version, event_id, source, event_type, payload, monotonic_ns, "
            "session_id, application, correlation_id FROM timeline_events WHERE 1=1";
        if (query.start_monotonic_ns) sql += " AND monotonic_ns >= ?";
        if (query.end_monotonic_ns) sql += " AND monotonic_ns < ?";
        if (query.session_id) sql += " AND session_id = ?";
        if (query.source) sql += " AND source = ?";
        if (query.application) sql += " AND application = ?";
        if (query.correlation_id) sql += " AND correlation_id = ?";
        if (query.event_type) sql += " AND event_type = ?";
        sql += " ORDER BY monotonic_ns ASC, sequence ASC LIMIT ? OFFSET ?";

        Statement statement(database_, sql);
        int parameter = 1;
        if (query.start_monotonic_ns) statement.bind_int64(parameter++, static_cast<sqlite3_int64>(*query.start_monotonic_ns));
        if (query.end_monotonic_ns) statement.bind_int64(parameter++, static_cast<sqlite3_int64>(*query.end_monotonic_ns));
        if (query.session_id) statement.bind_text(parameter++, *query.session_id);
        if (query.source) statement.bind_text(parameter++, *query.source);
        if (query.application) statement.bind_text(parameter++, *query.application);
        if (query.correlation_id) statement.bind_text(parameter++, *query.correlation_id);
        if (query.event_type) statement.bind_text(parameter++, *query.event_type);
        statement.bind_int64(parameter++, static_cast<sqlite3_int64>(page_size + 1));
        statement.bind_int64(parameter, static_cast<sqlite3_int64>(offset));

        TimelinePage page;
        while (statement.step() == SQLITE_ROW) {
            page.events.push_back(read_record(statement));
        }
        if (statement.last_result() != SQLITE_DONE) {
            throw TimelineStoreError(sqlite_error(database_, "cannot query timeline events"));
        }
        if (page.events.size() > page_size) {
            page.events.pop_back();
            page.has_more = true;
        }
        page.next_offset = offset + page.events.size();
        return page;
    }

    std::vector<CanonicalEvent> replay(const TimelineQuery& query = {}) const {
        std::vector<CanonicalEvent> events;
        for_each_record(query, [&](const TimelineRecord& record) { events.push_back(record.event); });
        return events;
    }

    std::vector<CanonicalEvent> replay_from(const std::string& last_applied_event_id) const {
        std::string sql = "SELECT monotonic_ns, sequence FROM timeline_events WHERE event_id = ?";
        Statement statement(database_, sql);
        statement.bind_text(1, last_applied_event_id);
        
        std::uint64_t start_ns = 0;
        std::uint64_t start_seq = 0;
        if (statement.step() == SQLITE_ROW) {
            start_ns = static_cast<std::uint64_t>(statement.column_int64(0));
            start_seq = static_cast<std::uint64_t>(statement.column_int64(1));
        } else {
            // Not found, replay from beginning
            return replay();
        }
        
        std::vector<CanonicalEvent> events;
        std::string fetch_sql =
            "SELECT sequence, schema_version, event_id, source, event_type, payload, monotonic_ns, "
            "session_id, application, correlation_id FROM timeline_events "
            "WHERE monotonic_ns > ? OR (monotonic_ns = ? AND sequence > ?) "
            "ORDER BY monotonic_ns ASC, sequence ASC";
            
        Statement fetch_stmt(database_, fetch_sql);
        fetch_stmt.bind_int64(1, static_cast<sqlite3_int64>(start_ns));
        fetch_stmt.bind_int64(2, static_cast<sqlite3_int64>(start_ns));
        fetch_stmt.bind_int64(3, static_cast<sqlite3_int64>(start_seq));
        
        while (fetch_stmt.step() == SQLITE_ROW) {
            events.push_back(read_record(fetch_stmt).event);
        }
        return events;
    }

    std::string export_json(const TimelineQuery& query = {}) const {
        std::ostringstream output;
        output << "[";
        bool first = true;
        for_each_record(query, [&](const TimelineRecord& record) {
            if (!first) output << ',';
            first = false;
            output << "{\"sequence\":" << record.sequence
                   << ",\"event_id\":\"" << json_escape(record.event.event_id)
                   << "\",\"schema_version\":\"" << json_escape(record.event.schema_version)
                   << "\",\"source\":\"" << json_escape(record.event.source)
                   << "\",\"event_type\":\"" << json_escape(record.event.event_type)
                   << "\",\"payload\":" << record.event.payload
                   << ",\"monotonic_ns\":" << record.event.monotonic_ns
                   << ",\"session_id\":\"" << json_escape(record.metadata.session_id)
                   << "\",\"application\":\"" << json_escape(record.metadata.application)
                   << "\",\"correlation_id\":\"" << json_escape(record.metadata.correlation_id)
                   << "\"}";
        });
        output << "]";
        return output.str();
    }

    void export_json(const std::string& path, const TimelineQuery& query = {}) const {
        std::ofstream output(path, std::ios::trunc);
        if (!output) throw TimelineStoreError("cannot open timeline export: " + path);
        output << export_json(query);
        if (!output) throw TimelineStoreError("cannot write timeline export: " + path);
    }

private:
    class Statement {
    public:
        Statement(sqlite3* database, const std::string& sql) : database_(database) {
            const int result = sqlite3_prepare_v2(database_, sql.c_str(), -1, &statement_, nullptr);
            if (result != SQLITE_OK) throw TimelineStoreError(sqlite_error(database_, "cannot prepare SQLite statement"));
        }

        ~Statement() {
            if (statement_ != nullptr) sqlite3_finalize(statement_);
        }

        void bind_text(int index, const std::string& value) {
            const int result = sqlite3_bind_text(statement_, index, value.c_str(), -1, nullptr);
            if (result != SQLITE_OK) throw TimelineStoreError(sqlite_error(database_, "cannot bind SQLite text"));
        }

        void bind_int64(int index, sqlite3_int64 value) {
            const int result = sqlite3_bind_int64(statement_, index, value);
            if (result != SQLITE_OK) throw TimelineStoreError(sqlite_error(database_, "cannot bind SQLite integer"));
        }

        int step() {
            last_result_ = sqlite3_step(statement_);
            return last_result_;
        }

        int last_result() const { return last_result_; }

        sqlite3_stmt* stmt() const { return statement_; }

        sqlite3_int64 column_int64(int index) const { return sqlite3_column_int64(statement_, index); }

        std::string column_text(int index) const {
            const auto* value = sqlite3_column_text(statement_, index);
            return value == nullptr ? std::string{} : reinterpret_cast<const char*>(value);
        }

    private:
        sqlite3* database_;
        sqlite3_stmt* statement_{nullptr};
        int last_result_{SQLITE_OK};
    };

    static std::string sqlite_error(sqlite3* database, const std::string& prefix) {
        return prefix + ": " + (database == nullptr ? "unknown SQLite error" : sqlite3_errmsg(database));
    }

    void exec(const std::string& sql) {
        const int result = sqlite3_exec(database_, sql.c_str(), nullptr, nullptr, nullptr);
        if (result != SQLITE_OK) throw TimelineStoreError(sqlite_error(database_, "cannot execute SQLite migration"));
    }

    void migrate() {
        const int current = schema_version_before_tables();
        if (current > 2) throw TimelineStoreError("unsupported timeline schema version");
        
        try {
            exec("BEGIN IMMEDIATE");
            if (current < 1) {
                exec("CREATE TABLE IF NOT EXISTS timeline_events ("
                     "sequence INTEGER PRIMARY KEY AUTOINCREMENT,"
                     "event_id TEXT NOT NULL UNIQUE,"
                     "schema_version TEXT NOT NULL,"
                     "source TEXT NOT NULL,"
                     "event_type TEXT NOT NULL,"
                     "payload TEXT NOT NULL,"
                     "monotonic_ns INTEGER NOT NULL,"
                     "session_id TEXT NOT NULL DEFAULT '',"
                     "application TEXT NOT NULL DEFAULT '',"
                     "correlation_id TEXT NOT NULL DEFAULT '')");
                exec("CREATE INDEX IF NOT EXISTS timeline_events_time_idx "
                     "ON timeline_events(monotonic_ns, sequence)");
                exec("CREATE INDEX IF NOT EXISTS timeline_events_context_idx "
                     "ON timeline_events(session_id, application, correlation_id, sequence)");
                exec("PRAGMA user_version = 1");
            }
            if (current < 2) {
                exec("CREATE TABLE IF NOT EXISTS cognitive_snapshots ("
                     "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                     "created_at INTEGER NOT NULL,"
                     "payload BLOB NOT NULL)");
                exec("PRAGMA user_version = 2");
            }
            exec("COMMIT");
        } catch (...) {
            try { exec("ROLLBACK"); } catch (...) {}
            throw;
        }
    }

    int schema_version_before_tables() const {
        Statement statement(database_, "PRAGMA user_version");
        if (statement.step() != SQLITE_ROW) throw TimelineStoreError(sqlite_error(database_, "cannot read SQLite schema version"));
        return static_cast<int>(statement.column_int64(0));
    }

    static TimelineRecord read_record(Statement& statement) {
        TimelineRecord record;
        record.sequence = static_cast<std::uint64_t>(statement.column_int64(0));
        record.event.schema_version = statement.column_text(1);
        record.event.event_id = statement.column_text(2);
        record.event.source = statement.column_text(3);
        record.event.event_type = statement.column_text(4);
        record.event.payload = statement.column_text(5);
        record.event.monotonic_ns = static_cast<std::size_t>(statement.column_int64(6));
        record.metadata.session_id = statement.column_text(7);
        record.metadata.application = statement.column_text(8);
        record.metadata.correlation_id = statement.column_text(9);
        return record;
    }

    template <typename Callback>
    void for_each_record(const TimelineQuery& query, Callback callback) const {
        std::size_t offset = 0;
        while (true) {
            const TimelinePage page = this->query(query, 256, offset);
            for (const auto& record : page.events) callback(record);
            if (!page.has_more) return;
            offset = page.next_offset;
        }
    }

    static std::string json_escape(const std::string& value) {
        std::string escaped;
        escaped.reserve(value.size());
        for (const char character : value) {
            if (character == '\\' || character == '"') escaped.push_back('\\');
            if (character == '\n') escaped += "\\n";
            else if (character == '\r') escaped += "\\r";
            else if (character == '\t') escaped += "\\t";
            else escaped.push_back(character);
        }
        return escaped;
    }

    sqlite3* database_{nullptr};
};

}  // namespace eu_digital
