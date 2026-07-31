#pragma once

// The deployed runtime uses the system SQLite library. Some research/build
// environments expose the shared library without installing sqlite3.h, so the
// small fallback declaration below keeps the integration auditable and limits
// the ABI surface to the calls used by TimelineStore.
#if __has_include(<sqlite3.h>)
#include <sqlite3.h>
#else

#include <cstdint>

#if defined(_WIN32) || defined(__WIN32__)
#include <windows.h>
#include <stdexcept>
#include <string>

inline HMODULE get_sqlite3_module() {
    static HMODULE module = []() {
        HMODULE h = LoadLibraryA("sqlite3.dll");
        if (!h) throw std::runtime_error("Failed to load sqlite3.dll dynamically.");
        return h;
    }();
    return module;
}

template<typename T>
inline T get_sqlite3_proc(const char* name) {
    auto proc = GetProcAddress(get_sqlite3_module(), name);
    if (!proc) throw std::runtime_error(std::string("Failed to find SQLite function: ") + name);
    return reinterpret_cast<T>(proc);
}

struct sqlite3;
struct sqlite3_stmt;

using sqlite3_int64 = std::int64_t;
using sqlite3_destructor_type = void (*)(void*);

inline int sqlite3_open_v2(const char* a, sqlite3** b, int c, const char* d) { return get_sqlite3_proc<int(*)(const char*, sqlite3**, int, const char*)>("sqlite3_open_v2")(a, b, c, d); }
inline int sqlite3_close(sqlite3* a) { return get_sqlite3_proc<int(*)(sqlite3*)>("sqlite3_close")(a); }
inline const char* sqlite3_errmsg(sqlite3* a) { return get_sqlite3_proc<const char*(*)(sqlite3*)>("sqlite3_errmsg")(a); }
inline int sqlite3_busy_timeout(sqlite3* a, int b) { return get_sqlite3_proc<int(*)(sqlite3*, int)>("sqlite3_busy_timeout")(a, b); }
inline int sqlite3_exec(sqlite3* a, const char* b, int (*c)(void*, int, char**, char**), void* d, char** e) { return get_sqlite3_proc<int(*)(sqlite3*, const char*, int (*)(void*, int, char**, char**), void*, char**)>("sqlite3_exec")(a, b, c, d, e); }
inline int sqlite3_prepare_v2(sqlite3* a, const char* b, int c, sqlite3_stmt** d, const char** e) { return get_sqlite3_proc<int(*)(sqlite3*, const char*, int, sqlite3_stmt**, const char**)>("sqlite3_prepare_v2")(a, b, c, d, e); }
inline int sqlite3_bind_text(sqlite3_stmt* a, int b, const char* c, int d, sqlite3_destructor_type e) { return get_sqlite3_proc<int(*)(sqlite3_stmt*, int, const char*, int, sqlite3_destructor_type)>("sqlite3_bind_text")(a, b, c, d, e); }
inline int sqlite3_bind_int64(sqlite3_stmt* a, int b, sqlite3_int64 c) { return get_sqlite3_proc<int(*)(sqlite3_stmt*, int, sqlite3_int64)>("sqlite3_bind_int64")(a, b, c); }
inline int sqlite3_bind_blob(sqlite3_stmt* a, int b, const void* c, int d, sqlite3_destructor_type e) { return get_sqlite3_proc<int(*)(sqlite3_stmt*, int, const void*, int, sqlite3_destructor_type)>("sqlite3_bind_blob")(a, b, c, d, e); }
inline int sqlite3_step(sqlite3_stmt* a) { return get_sqlite3_proc<int(*)(sqlite3_stmt*)>("sqlite3_step")(a); }
inline int sqlite3_finalize(sqlite3_stmt* a) { return get_sqlite3_proc<int(*)(sqlite3_stmt*)>("sqlite3_finalize")(a); }
inline const unsigned char* sqlite3_column_text(sqlite3_stmt* a, int b) { return get_sqlite3_proc<const unsigned char*(*)(sqlite3_stmt*, int)>("sqlite3_column_text")(a, b); }
inline sqlite3_int64 sqlite3_column_int64(sqlite3_stmt* a, int b) { return get_sqlite3_proc<sqlite3_int64(*)(sqlite3_stmt*, int)>("sqlite3_column_int64")(a, b); }
inline const void* sqlite3_column_blob(sqlite3_stmt* a, int b) { return get_sqlite3_proc<const void*(*)(sqlite3_stmt*, int)>("sqlite3_column_blob")(a, b); }
inline int sqlite3_column_bytes(sqlite3_stmt* a, int b) { return get_sqlite3_proc<int(*)(sqlite3_stmt*, int)>("sqlite3_column_bytes")(a, b); }

#else

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
int sqlite3_bind_blob(sqlite3_stmt*, int, const void*, int, sqlite3_destructor_type);
int sqlite3_step(sqlite3_stmt*);
int sqlite3_finalize(sqlite3_stmt*);
const unsigned char* sqlite3_column_text(sqlite3_stmt*, int);
sqlite3_int64 sqlite3_column_int64(sqlite3_stmt*, int);
const void* sqlite3_column_blob(sqlite3_stmt*, int);
int sqlite3_column_bytes(sqlite3_stmt*, int);
}

#endif

constexpr int SQLITE_OK = 0;
constexpr int SQLITE_ROW = 100;
constexpr int SQLITE_DONE = 101;
constexpr int SQLITE_ERROR = 1;
constexpr int SQLITE_CONSTRAINT = 19;
constexpr int SQLITE_OPEN_READWRITE = 0x00000002;
constexpr int SQLITE_OPEN_CREATE = 0x00000004;

#define SQLITE_TRANSIENT ((sqlite3_destructor_type)-1)

#endif
