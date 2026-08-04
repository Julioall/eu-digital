#pragma once

#include "core/cognitive_snapshot.hpp"
#include "core/privacy_storage.hpp"
#include "core/timeline_store.hpp"

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace eu_digital {

struct CognitiveSnapshotWriterStats {
    std::size_t submitted{0};
    std::size_t written{0};
    std::size_t replaced{0};
    std::size_t failures{0};
    std::size_t largest_plaintext_bytes{0};
};

struct CognitiveSnapshotWriteRequest {
    std::string captured_at;
    double captured_epoch_seconds{0.0};
    std::string configuration_fingerprint;
    std::string last_applied_event_id;
    contracts::CognitiveStateBundleV1 state;
    std::int64_t created_at_ns{0};
};

class AsyncCognitiveSnapshotWriter {
public:
    using ErrorSink = std::function<void(const std::string&)>;

    explicit AsyncCognitiveSnapshotWriter(
        std::string timeline_path, ErrorSink error_sink = {},
        std::size_t max_plaintext_bytes = 4U * 1024U * 1024U)
        : timeline_path_(std::move(timeline_path)),
          error_sink_(std::move(error_sink)),
          max_plaintext_bytes_(max_plaintext_bytes),
          worker_(&AsyncCognitiveSnapshotWriter::run, this) {
        if (timeline_path_.empty() || max_plaintext_bytes_ == 0) {
            throw std::invalid_argument(
                "snapshot timeline path and size limit are required");
        }
    }

    ~AsyncCognitiveSnapshotWriter() { stop(); }

    AsyncCognitiveSnapshotWriter(const AsyncCognitiveSnapshotWriter&) = delete;
    AsyncCognitiveSnapshotWriter& operator=(
        const AsyncCognitiveSnapshotWriter&) = delete;

    bool submit(std::string plaintext, std::int64_t created_at_ns) {
        if (plaintext.empty() || plaintext.size() > max_plaintext_bytes_) {
            return false;
        }
        {
            std::lock_guard lock(mutex_);
            if (stopping_) return false;
            ++stats_.submitted;
            stats_.largest_plaintext_bytes = std::max(
                stats_.largest_plaintext_bytes, plaintext.size());
            if (pending_) ++stats_.replaced;
            Pending item;
            item.plaintext = std::move(plaintext);
            item.created_at_ns = created_at_ns;
            pending_ = std::move(item);
        }
        ready_.notify_one();
        return true;
    }

    bool submit(CognitiveSnapshotWriteRequest request) {
        if (request.captured_at.empty() ||
            request.configuration_fingerprint.empty() ||
            request.last_applied_event_id.empty() || !request.state.valid()) {
            return false;
        }
        {
            std::lock_guard lock(mutex_);
            if (stopping_) return false;
            ++stats_.submitted;
            if (pending_) ++stats_.replaced;
            Pending item;
            item.request = std::move(request);
            item.created_at_ns = item.request->created_at_ns;
            pending_ = std::move(item);
        }
        ready_.notify_one();
        return true;
    }

    void wait_idle() {
        std::unique_lock lock(mutex_);
        idle_.wait(lock, [this] { return !pending_ && !writing_; });
    }

    void stop() {
        {
            std::lock_guard lock(mutex_);
            if (stopping_ && !worker_.joinable()) return;
            stopping_ = true;
        }
        ready_.notify_all();
        if (worker_.joinable()) worker_.join();
    }

    CognitiveSnapshotWriterStats stats() const {
        std::lock_guard lock(mutex_);
        return stats_;
    }

    std::size_t pending_count() const {
        std::lock_guard lock(mutex_);
        return pending_ ? 1U : 0U;
    }

private:
    struct Pending {
        std::string plaintext;
        std::optional<CognitiveSnapshotWriteRequest> request;
        std::int64_t created_at_ns{0};
    };

    void run() {
        while (true) {
            Pending item;
            {
                std::unique_lock lock(mutex_);
                ready_.wait(lock, [this] { return stopping_ || pending_; });
                if (!pending_) {
                    if (stopping_) break;
                    continue;
                }
                item = std::move(*pending_);
                pending_.reset();
                writing_ = true;
            }
            try {
                if (item.request) {
                    auto snapshot = CognitiveSnapshotV2::create(
                        std::move(item.request->captured_at),
                        item.request->captured_epoch_seconds,
                        std::move(item.request->configuration_fingerprint),
                        std::move(item.request->last_applied_event_id),
                        std::move(item.request->state));
                    item.plaintext = snapshot.to_json();
                    if (item.plaintext.size() > max_plaintext_bytes_) {
                        throw std::runtime_error(
                            "cognitive snapshot exceeds plaintext size limit");
                    }
                    std::lock_guard lock(mutex_);
                    stats_.largest_plaintext_bytes = std::max(
                        stats_.largest_plaintext_bytes,
                        item.plaintext.size());
                }
                const std::vector<std::uint8_t> plaintext(
                    item.plaintext.begin(), item.plaintext.end());
                const auto encrypted = LocalDataProtection::protect(plaintext);
                TimelineStore store(timeline_path_);
                store.save_snapshot(encrypted, item.created_at_ns);
                std::lock_guard lock(mutex_);
                ++stats_.written;
            } catch (const std::exception& error) {
                ErrorSink sink;
                {
                    std::lock_guard lock(mutex_);
                    ++stats_.failures;
                    sink = error_sink_;
                }
                if (sink) {
                    try { sink(error.what()); } catch (...) {}
                }
            }
            item.plaintext.clear();
            {
                std::lock_guard lock(mutex_);
                writing_ = false;
                if (!pending_) idle_.notify_all();
                if (stopping_ && !pending_) ready_.notify_all();
            }
        }
        std::lock_guard lock(mutex_);
        writing_ = false;
        idle_.notify_all();
    }

    std::string timeline_path_;
    ErrorSink error_sink_;
    std::size_t max_plaintext_bytes_;
    mutable std::mutex mutex_;
    std::condition_variable ready_;
    std::condition_variable idle_;
    std::optional<Pending> pending_;
    bool writing_{false};
    bool stopping_{false};
    CognitiveSnapshotWriterStats stats_;
    std::thread worker_;
};

}  // namespace eu_digital
