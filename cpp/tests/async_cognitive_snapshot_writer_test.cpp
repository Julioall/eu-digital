#include "core/async_cognitive_snapshot_writer.hpp"

#include <cassert>
#include <chrono>
#include <filesystem>
#include <string>

using namespace eu_digital;

int main() {
    namespace fs = std::filesystem;
    if (!LocalDataProtection::available()) return 0;

    const auto root = fs::temp_directory_path() /
        "eu_digital_async_cognitive_snapshot_writer_test";
    fs::remove_all(root);
    fs::create_directories(root);
    const auto timeline = root / "timeline.sqlite";
    {
        TimelineStore initialize(timeline.string());
    }

    std::string error;
    AsyncCognitiveSnapshotWriter writer(
        timeline.string(), [&](const std::string& value) { error = value; });
    const std::string payload = "{\"schema_version\":\"2.0\"}";
    const auto started = std::chrono::steady_clock::now();
    assert(writer.submit(payload, 100));
    const auto submit_duration = std::chrono::steady_clock::now() - started;
    assert(submit_duration < std::chrono::milliseconds(5));
    assert(writer.pending_count() <= 1);
    writer.wait_idle();
    writer.stop();
    assert(error.empty());
    const auto stats = writer.stats();
    assert(stats.submitted == 1);
    assert(stats.written == 1);
    assert(stats.failures == 0);
    assert(stats.largest_plaintext_bytes == payload.size());

    AsyncCognitiveSnapshotWriter bounded(timeline.string(), {}, 8);
    assert(!bounded.submit(std::string(9, 'x'), 101));
    bounded.stop();

    {
        TimelineStore verify(timeline.string());
        const auto records = verify.load_snapshot_records();
        assert(records.size() == 1);
        const auto plaintext =
            LocalDataProtection::unprotect(records.front().encrypted_payload);
        assert(std::string(plaintext.begin(), plaintext.end()) == payload);
    }
    fs::remove_all(root);
}
