#pragma once

#include "core/event_bus.hpp"
#include "core/capability_runtime.hpp"
#include "core/ports/iepisode_boundary_port.hpp"
#include "core/ports/imemory_write_port.hpp"
#include "core/ports/iprediction_port.hpp"
#include "core/ports/iworkspace_selection_port.hpp"
#include "core/ports/imetacognition_port.hpp"
#include "core/ports/iself_model_query_port.hpp"
#include "core/ports/icognitive_decision_port.hpp"

#include <atomic>
#include <condition_variable>
#include <deque>
#include <iostream>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace eu_digital {

enum class CycleState {
    queued,
    processing,
    degraded,
    completed,
    failed,
    discarded
};

struct CycleStatusLog {
    std::string event_id;
    CycleState state;
    std::string reason;
};

class CognitiveCoordinator {
public:
    explicit CognitiveCoordinator(const CapabilityRegistry& registry, std::size_t max_queue_size = 100)
        : registry_(registry), max_queue_size_(max_queue_size), active_(true) {
        worker_ = std::thread(&CognitiveCoordinator::process_loop, this);
    }

    ~CognitiveCoordinator() {
        stop();
    }

    CognitiveCoordinator(const CognitiveCoordinator&) = delete;
    CognitiveCoordinator& operator=(const CognitiveCoordinator&) = delete;

    void enqueue(const CanonicalEvent& event) {
        std::unique_lock<std::mutex> lock(mutex_);
        if (!active_) return;

        if (queue_.size() >= max_queue_size_) {
            // Backpressure: drop event
            std::lock_guard<std::mutex> log_lock(log_mutex_);
            logs_.push_back({event.event_id, CycleState::discarded, "queue_full_backpressure"});
            return;
        }

        queue_.push_back(event);
        {
            std::lock_guard<std::mutex> log_lock(log_mutex_);
            logs_.push_back({event.event_id, CycleState::queued, ""});
        }
        condition_.notify_one();
    }

    void stop() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            active_ = false;
        }
        condition_.notify_all();
        if (worker_.joinable()) {
            worker_.join();
        }
    }

    std::vector<CycleStatusLog> get_logs() const {
        std::lock_guard<std::mutex> log_lock(log_mutex_);
        return logs_;
    }

private:
    void process_loop() {
        while (true) {
            CanonicalEvent event;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                condition_.wait(lock, [this] { return !queue_.empty() || !active_; });
                
                if (!active_ && queue_.empty()) break;
                
                event = queue_.front();
                queue_.pop_front();
            }

            log_state(event.event_id, CycleState::processing, "");

            try {
                process_event(event);
            } catch (const std::exception& e) {
                log_state(event.event_id, CycleState::failed, e.what());
            } catch (...) {
                log_state(event.event_id, CycleState::failed, "unknown_fatal_error");
            }
        }
    }

    void process_event(const CanonicalEvent& event) {
        bool degraded = false;
        std::string degradation_reason;

        // 1. Episode Boundary
        auto episode_port = registry_.resolve<IEpisodeBoundaryPort>("episode_boundary");
        if (episode_port) {
            try {
                auto episode_update = episode_port->evaluate(event);
            } catch (const std::exception& e) {
                degraded = true;
                degradation_reason += std::string("episode_error: ") + e.what() + "; ";
            }
        } else {
            degraded = true;
            degradation_reason += "missing_episode_port; ";
        }

        // 2. Memory Write
        auto memory_port = registry_.resolve<IMemoryWritePort>("memory_write");
        if (memory_port) {
            try {
                auto mem_result = memory_port->store_event(event);
            } catch (const std::exception& e) {
                degraded = true;
                degradation_reason += std::string("memory_error: ") + e.what() + "; ";
            }
        } else {
            degraded = true;
            degradation_reason += "missing_memory_port; ";
        }

        // 3. Prediction
        auto prediction_port = registry_.resolve<IPredictionPort>("prediction");
        if (prediction_port) {
            try {
                auto pred_result = prediction_port->predict({event.payload}, "now");
            } catch (const std::exception& e) {
                degraded = true;
                degradation_reason += std::string("prediction_error: ") + e.what() + "; ";
            }
        } else {
            degraded = true;
            degradation_reason += "missing_prediction_port; ";
        }

        // 4. Workspace
        auto workspace_port = registry_.resolve<IWorkspaceSelectionPort>("workspace");
        contracts::WorkspaceSnapshot workspace_snapshot;
        if (workspace_port) {
            try {
                workspace_snapshot = workspace_port->select(event);
            } catch (const std::exception& e) {
                degraded = true;
                degradation_reason += std::string("workspace_error: ") + e.what() + "; ";
            }
        } else {
            degraded = true;
            degradation_reason += "missing_workspace_port; ";
        }

        // 5. Metacognition
        auto metacog_port = registry_.resolve<IMetacognitionPort>("metacognition");
        if (metacog_port) {
            try {
                auto metacog_result = metacog_port->evaluate(workspace_snapshot);
            } catch (const std::exception& e) {
                degraded = true;
                degradation_reason += std::string("metacognition_error: ") + e.what() + "; ";
            }
        } else {
            degraded = true;
            degradation_reason += "missing_metacognition_port; ";
        }

        // 6. Self Model
        auto self_model_port = registry_.resolve<ISelfModelQueryPort>("self_model");
        if (self_model_port) {
            try {
                auto self_result = self_model_port->query_constraints();
            } catch (const std::exception& e) {
                degraded = true;
                degradation_reason += std::string("self_model_error: ") + e.what() + "; ";
            }
        } else {
            degraded = true;
            degradation_reason += "missing_self_model_port; ";
        }

        // 7. Decision
        auto decision_port = registry_.resolve<ICognitiveDecisionPort>("decision");
        if (decision_port) {
            try {
                auto decision_result = decision_port->decide(event);
            } catch (const std::exception& e) {
                degraded = true;
                degradation_reason += std::string("decision_error: ") + e.what() + "; ";
            }
        } else {
            degraded = true;
            degradation_reason += "missing_decision_port; ";
        }

        if (degraded) {
            log_state(event.event_id, CycleState::degraded, degradation_reason);
        }
        
        log_state(event.event_id, CycleState::completed, "");
    }

    void log_state(const std::string& event_id, CycleState state, const std::string& reason) {
        std::lock_guard<std::mutex> log_lock(log_mutex_);
        logs_.push_back({event_id, state, reason});
    }

    const CapabilityRegistry& registry_;
    std::size_t max_queue_size_;

    std::deque<CanonicalEvent> queue_;
    std::mutex mutex_;
    std::condition_variable condition_;
    bool active_;
    std::thread worker_;

    mutable std::mutex log_mutex_;
    std::vector<CycleStatusLog> logs_;
};

} // namespace eu_digital
