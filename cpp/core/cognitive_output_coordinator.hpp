#pragma once

#include "core/capability_runtime.hpp"
#include "core/contracts/cognitive_output.hpp"
#include "core/ports/ilanguage_renderer.hpp"
#include "core/ports/ipresentation_port.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <set>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace eu_digital {

struct CognitiveOutputCoordinatorConfig {
    std::size_t max_queue_size{32};
    bool auto_start{true};
};

struct CognitiveOutputEnqueueResult {
    bool accepted{false};
    std::string reason_code;
    std::uint64_t duration_microseconds{0};
};

struct CognitiveOutputLog {
    std::string request_id;
    std::string stage;
    std::string status;
    std::string reason_code;
};

struct CognitiveOutputMetrics {
    std::uint64_t enqueued{0};
    std::uint64_t duplicates{0};
    std::uint64_t backpressure{0};
    std::uint64_t renderer_unavailable{0};
    std::uint64_t renderer_failures{0};
    std::uint64_t rendered{0};
    std::uint64_t fallback_used{0};
    std::uint64_t silence{0};
    std::uint64_t presentation_unavailable{0};
    std::uint64_t presentation_failures{0};
    std::uint64_t presented{0};
};

class CognitiveOutputCoordinator {
public:
    explicit CognitiveOutputCoordinator(
        const CapabilityRegistry& registry,
        CognitiveOutputCoordinatorConfig config = {})
        : registry_(registry), config_(config) {
        if (config_.max_queue_size == 0) {
            throw std::invalid_argument("output queue size must be positive");
        }
        if (config_.auto_start) start();
    }

    ~CognitiveOutputCoordinator() { stop(); }

    CognitiveOutputCoordinator(const CognitiveOutputCoordinator&) = delete;
    CognitiveOutputCoordinator& operator=(const CognitiveOutputCoordinator&) = delete;

    void start() {
        std::lock_guard lock(queue_mutex_);
        if (started_) return;
        if (stopping_) {
            throw std::logic_error("output coordinator cannot restart after stop");
        }
        started_ = true;
        worker_ = std::thread(&CognitiveOutputCoordinator::process_loop, this);
    }

    CognitiveOutputEnqueueResult enqueue(
        const contracts::CognitiveOutputRequestV1& request) {
        const auto started_at = std::chrono::steady_clock::now();
        CognitiveOutputEnqueueResult result;
        if (!request.valid()) {
            result.reason_code = "invalid_request";
            result.duration_microseconds = elapsed(started_at);
            return result;
        }
        {
            std::lock_guard lock(queue_mutex_);
            if (!started_ || stopping_) {
                result.reason_code = "coordinator_stopped";
            } else if (accepted_ids_.contains(request.request_id)) {
                ++duplicates_;
                result.reason_code = "duplicate_request";
            } else if (queue_.size() >= config_.max_queue_size) {
                ++backpressure_;
                result.reason_code = "discarded_backpressure";
            } else {
                accepted_ids_.insert(request.request_id);
                queue_.push_back(request);
                ++enqueued_;
                result.accepted = true;
                result.reason_code = "accepted";
            }
        }
        result.duration_microseconds = elapsed(started_at);
        if (result.accepted) queue_ready_.notify_one();
        return result;
    }

    void wait_idle() {
        std::unique_lock lock(queue_mutex_);
        idle_.wait(lock, [&] { return queue_.empty() && in_flight_ == 0; });
    }

    void stop() {
        {
            std::lock_guard lock(queue_mutex_);
            if (!started_ || stopping_) return;
            stopping_ = true;
        }
        queue_ready_.notify_all();
        if (worker_.joinable()) worker_.join();
        std::lock_guard lock(queue_mutex_);
        started_ = false;
    }

    CognitiveOutputMetrics metrics() const {
        return {
            enqueued_.load(),
            duplicates_.load(),
            backpressure_.load(),
            renderer_unavailable_.load(),
            renderer_failures_.load(),
            rendered_.load(),
            fallback_used_.load(),
            silence_.load(),
            presentation_unavailable_.load(),
            presentation_failures_.load(),
            presented_.load(),
        };
    }

    std::vector<CognitiveOutputLog> logs() const {
        std::lock_guard lock(log_mutex_);
        return logs_;
    }

private:
    static std::uint64_t elapsed(
        std::chrono::steady_clock::time_point started_at) {
        return static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now() - started_at)
                .count());
    }

    void process_loop() {
        while (true) {
            contracts::CognitiveOutputRequestV1 request;
            {
                std::unique_lock lock(queue_mutex_);
                queue_ready_.wait(lock,
                                  [&] { return stopping_ || !queue_.empty(); });
                if (stopping_ && queue_.empty()) break;
                request = std::move(queue_.front());
                queue_.pop_front();
                ++in_flight_;
            }
            process(request);
            {
                std::lock_guard lock(queue_mutex_);
                --in_flight_;
                if (queue_.empty() && in_flight_ == 0) idle_.notify_all();
            }
        }
        std::lock_guard lock(queue_mutex_);
        if (queue_.empty() && in_flight_ == 0) idle_.notify_all();
    }

    void process(const contracts::CognitiveOutputRequestV1& request) {
        const auto renderer =
            registry_.resolve<ILanguageRenderer>(kLanguageRenderOperation);
        if (!renderer) {
            ++renderer_unavailable_;
            log(request.request_id, "render", "skipped_unavailable",
                "renderer_unavailable");
            return;
        }

        contracts::ValidatedDialogueOutputV1 output;
        try {
            output = renderer->render(request);
        } catch (const std::exception&) {
            ++renderer_failures_;
            log(request.request_id, "render", "failed",
                "renderer_exception");
            return;
        } catch (...) {
            ++renderer_failures_;
            log(request.request_id, "render", "failed",
                "renderer_unknown_exception");
            return;
        }

        if (!output.valid() || output.request_id != request.request_id ||
            output.intent != request.intent ||
            !evidence_is_subset(output.evidence_refs, request.evidence_refs)) {
            ++renderer_failures_;
            log(request.request_id, "render", "failed",
                "renderer_contract_violation");
            return;
        }

        switch (output.status) {
        case contracts::DialogueOutputStatusV1::rendered: ++rendered_; break;
        case contracts::DialogueOutputStatusV1::fallback_used:
            ++fallback_used_;
            break;
        default: ++silence_; break;
        }
        log(request.request_id, "render", "succeeded", output.reason_code);
        if (!output.presentable()) return;

        const auto presentation =
            registry_.resolve<IPresentationPort>(kPresentationOperation);
        if (!presentation) {
            ++presentation_unavailable_;
            log(request.request_id, "presentation", "skipped_unavailable",
                "presentation_unavailable");
            return;
        }
        contracts::PortResult<bool> result;
        try {
            result = presentation->present(output);
        } catch (const std::exception&) {
            ++presentation_failures_;
            log(request.request_id, "presentation", "failed",
                "presentation_exception");
            return;
        } catch (...) {
            ++presentation_failures_;
            log(request.request_id, "presentation", "failed",
                "presentation_unknown_exception");
            return;
        }
        if (!result.valid() || !result.success || !result.value ||
            !*result.value) {
            ++presentation_failures_;
            const auto reason = result.error && !result.error->code.empty()
                ? result.error->code
                : "presentation_contract_violation";
            log(request.request_id, "presentation", "failed", reason);
            return;
        }
        ++presented_;
        log(request.request_id, "presentation", "succeeded", "presented");
    }

    static bool evidence_is_subset(
        const std::vector<std::string>& candidate,
        const std::vector<std::string>& allowed) {
        return std::all_of(candidate.begin(), candidate.end(),
                           [&](const auto& reference) {
                               return std::find(allowed.begin(), allowed.end(),
                                                reference) != allowed.end();
                           });
    }

    void log(std::string request_id, std::string stage, std::string status,
             std::string reason_code) {
        std::lock_guard lock(log_mutex_);
        if (logs_.size() == 256) logs_.erase(logs_.begin());
        logs_.push_back({std::move(request_id), std::move(stage),
                         std::move(status), std::move(reason_code)});
    }

    const CapabilityRegistry& registry_;
    CognitiveOutputCoordinatorConfig config_;
    mutable std::mutex queue_mutex_;
    std::condition_variable queue_ready_;
    std::condition_variable idle_;
    std::deque<contracts::CognitiveOutputRequestV1> queue_;
    std::set<std::string> accepted_ids_;
    std::size_t in_flight_{0};
    bool started_{false};
    bool stopping_{false};
    std::thread worker_;

    mutable std::mutex log_mutex_;
    std::vector<CognitiveOutputLog> logs_;
    std::atomic<std::uint64_t> enqueued_{0};
    std::atomic<std::uint64_t> duplicates_{0};
    std::atomic<std::uint64_t> backpressure_{0};
    std::atomic<std::uint64_t> renderer_unavailable_{0};
    std::atomic<std::uint64_t> renderer_failures_{0};
    std::atomic<std::uint64_t> rendered_{0};
    std::atomic<std::uint64_t> fallback_used_{0};
    std::atomic<std::uint64_t> silence_{0};
    std::atomic<std::uint64_t> presentation_unavailable_{0};
    std::atomic<std::uint64_t> presentation_failures_{0};
    std::atomic<std::uint64_t> presented_{0};
};

}  // namespace eu_digital
