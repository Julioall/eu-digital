#pragma once

#include <algorithm>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <functional>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_set>
#include <utility>
#include <vector>

namespace eu_digital {

struct CanonicalEvent {
    std::string schema_version{"1.0"};
    std::string event_id;
    std::string source;
    std::string event_type;
    std::string payload;
    std::size_t monotonic_ns{};
    std::string occurred_at;
    std::string received_at;
    std::string session_id;

    bool valid() const {
        return schema_version == "1.0" && !event_id.empty() && !source.empty() && !event_type.empty();
    }
};

struct DeadLetter {
    std::size_t sequence{};
    std::optional<std::string> event_id;
    std::string reason;
    CanonicalEvent event;
};

enum class PublishResult { accepted, duplicate };

class EventBus {
public:
    using Handler = std::function<void(const CanonicalEvent&)>;

    explicit EventBus(std::size_t max_queue_size = 128) : max_queue_size_(max_queue_size) {
        if (max_queue_size_ == 0) throw std::invalid_argument("max_queue_size must be positive");
        worker_ = std::thread([this] { run(); });
    }

    ~EventBus() {
        {
            std::lock_guard lock(mutex_);
            stopping_ = true;
        }
        not_empty_.notify_one();
        not_full_.notify_all();
        idle_.notify_all();
        if (worker_.joinable()) worker_.join();
    }

    EventBus(const EventBus&) = delete;
    EventBus& operator=(const EventBus&) = delete;

    std::size_t subscribe(std::string event_type, std::string source, Handler handler) {
        std::lock_guard lock(mutex_);
        const auto token = next_token_++;
        subscriptions_.push_back({token, std::move(event_type), std::move(source), std::move(handler)});
        return token;
    }

    void unsubscribe(std::size_t token) {
        std::lock_guard lock(mutex_);
        subscriptions_.erase(std::remove_if(subscriptions_.begin(), subscriptions_.end(),
            [token](const Subscription& item) { return item.token == token; }), subscriptions_.end());
    }

    PublishResult publish(CanonicalEvent event) {
        std::unique_lock lock(mutex_);
        if (!event.valid()) {
            dead_letters_.push_back({++sequence_, event.event_id.empty() ? std::nullopt : std::optional{event.event_id},
                "invalid CanonicalEvent", std::move(event)});
            throw std::invalid_argument("invalid CanonicalEvent");
        }
        if (processed_.contains(event.event_id)) return PublishResult::duplicate;
        not_full_.wait(lock, [this] { return stopping_ || queue_.size() < max_queue_size_; });
        if (stopping_) throw std::runtime_error("event bus is stopping");
        processed_.insert(event.event_id);
        queue_.push_back(std::move(event));
        ++in_flight_;
        not_empty_.notify_one();
        return PublishResult::accepted;
    }

    void replay(const std::vector<CanonicalEvent>& events) {
        for (const auto& event : events) publish(event);
        wait_idle();
    }

    void wait_idle() {
        std::unique_lock lock(mutex_);
        idle_.wait(lock, [this] { return in_flight_ == 0; });
    }

    std::vector<DeadLetter> dead_letters() const {
        std::lock_guard lock(mutex_);
        return dead_letters_;
    }

private:
    struct Subscription { std::size_t token; std::string event_type; std::string source; Handler handler; };

    void run() {
        while (true) {
            CanonicalEvent event;
            {
                std::unique_lock lock(mutex_);
                not_empty_.wait(lock, [this] { return stopping_ || !queue_.empty(); });
                if (stopping_ && queue_.empty()) return;
                event = std::move(queue_.front());
                queue_.pop_front();
                not_full_.notify_one();
            }
            std::vector<Subscription> subscribers;
            {
                std::lock_guard lock(mutex_);
                subscribers = subscriptions_;
            }
            for (const auto& subscription : subscribers) {
                if ((subscription.event_type.empty() || subscription.event_type == event.event_type) &&
                    (subscription.source.empty() || subscription.source == event.source)) {
                    subscription.handler(event);
                }
            }
            {
                std::lock_guard lock(mutex_);
                if (--in_flight_ == 0) idle_.notify_all();
            }
        }
    }

    const std::size_t max_queue_size_;
    mutable std::mutex mutex_;
    std::condition_variable not_empty_, not_full_, idle_;
    std::deque<CanonicalEvent> queue_;
    std::vector<Subscription> subscriptions_;
    std::vector<DeadLetter> dead_letters_;
    std::unordered_set<std::string> processed_;
    std::thread worker_;
    std::size_t next_token_{1}, sequence_{0}, in_flight_{0};
    bool stopping_{false};
};

}  // namespace eu_digital
