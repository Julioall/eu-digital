#pragma once

// The deployed runtime uses the system SQLite library. Some research/build
// environments expose the shared library without installing sqlite3.h, so the
// small fallback declaration below keeps the integration auditable and limits
// the ABI surface to the calls used by TimelineStore.
#if __has_include(<sqlite3.h>)
#include <sqlite3.h>
#else

#include <cstdint>

extern "C" {
struct sqlite3;
struct sqlite3_stmt;

using sqlite3_int64 = std::int64_t;
using sqlite3_destructor_type = void (*)(void*);

int sqlite3_open_v2(const char*, sqlite3**, int, const char*);
int sqlite3_close(sqlite3*);
const char* sqlite3_errmsg(sqlite3*);
int sqlite3_busy_timeout(sqlite3*, int);
int sqlite3_exec(sqlite3*, const char*, int (*)(void*, int, char**, char**), void*, char**);
int sqlite3_prepare_v2(sqlite3*, const char*, int, sqlite3_stmt**, const char**);
int sqlite3_bind_text(sqlite3_stmt*, int, const char*, int, sqlite3_destructor_type);
int sqlite3_bind_int64(sqlite3_stmt*, int, sqlite3_int64);
int sqlite3_step(sqlite3_stmt*);
int sqlite3_finalize(sqlite3_stmt*);
const unsigned char* sqlite3_column_text(sqlite3_stmt*, int);
sqlite3_int64 sqlite3_column_int64(sqlite3_stmt*, int);
}

constexpr int SQLITE_OK = 0;
constexpr int SQLITE_ROW = 100;
constexpr int SQLITE_DONE = 101;
constexpr int SQLITE_ERROR = 1;
constexpr int SQLITE_CONSTRAINT = 19;
constexpr int SQLITE_OPEN_READWRITE = 0x00000002;
constexpr int SQLITE_OPEN_CREATE = 0x00000004;

#endif
