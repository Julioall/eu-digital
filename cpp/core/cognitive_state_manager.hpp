#pragma once

#include "core/capability_runtime.hpp"
#include "core/cognitive_coordinator.hpp"
#include "core/cognitive_snapshot.hpp"
#include "core/digest.hpp"
#include "core/ports/icognitive_state_port.hpp"

#include <algorithm>
#include <chrono>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace eu_digital {

inline constexpr std::string_view kStatefulCognitiveOperations[] = {
    "episode_boundary", "memory_write", "pattern_learning", "workspace",
    "metacognition", "self_model"};

inline std::string cognitive_configuration_fingerprint(
    const std::string& runtime_id, const std::string& runtime_version,
    const CapabilityRegistry& registry) {
    std::ostringstream canonical;
    canonical << "runtime=" << runtime_id << '@' << runtime_version << '\n';
    canonical << "profile=" << registry.active_profile().value_or("none") << '\n';
    for (const auto& [implementation_id, record] : registry.records()) {
        if (record.state.state != CapabilityState::available &&
            record.state.state != CapabilityState::degraded) {
            continue;
        }
        canonical << implementation_id << '@'
                  << record.descriptor.implementation_version << ':'
                  << record.priority << ':'
                  << (record.descriptor.supports_checkpoint ? '1' : '0') << ':';
        auto operations = record.descriptor.provides;
        std::sort(operations.begin(), operations.end(),
                  [](const auto& left, const auto& right) {
                      return left.operation < right.operation;
                  });
        for (const auto& operation : operations) {
            canonical << operation.operation << ',';
        }
        canonical << '\n';
    }
    return digest::hex(digest::sha256(canonical.str()));
}

class CognitiveStateManager {
public:
    CognitiveStateManager(
        const CapabilityRegistry& registry, CognitiveCoordinator& coordinator,
        std::chrono::milliseconds capture_budget = std::chrono::milliseconds(5))
        : registry_(registry), coordinator_(coordinator),
          capture_budget_(capture_budget) {
        if (capture_budget_ <= std::chrono::milliseconds::zero()) {
            throw std::invalid_argument("capture budget must be positive");
        }
    }

    std::optional<CognitiveSnapshotV2> capture(
        const std::string& captured_at, double captured_epoch_seconds,
        const std::string& configuration_fingerprint,
        const std::string& last_applied_event_id) const {
        auto bundle = capture_bundle(last_applied_event_id);
        if (!bundle) return std::nullopt;
        return CognitiveSnapshotV2::create(
            captured_at, captured_epoch_seconds, configuration_fingerprint,
            last_applied_event_id, std::move(*bundle));
    }

    std::optional<contracts::CognitiveStateBundleV1> capture_bundle(
        const std::string& correlation_id) const {
        const auto started = std::chrono::steady_clock::now();
        const auto deadline = started + capture_budget_;
        const auto required = required_provider_ids();
        if (!required) return std::nullopt;
        const auto ports = state_ports(*required);
        if (!ports) return std::nullopt;

        contracts::CognitiveStateBundleV1 bundle;
        bundle.coordinator = coordinator_.capture_checkpoint();
        bundle.required_provider_ids = *required;
        for (const auto& provider_id : *required) {
            contracts::PortInvocationContextV1 context;
            context.correlation_id = correlation_id;
            context.deadline = deadline;
            const auto result = ports->at(provider_id)->capture_state(context);
            if (!result.valid() || !result.success || !result.value ||
                !result.value->valid() ||
                result.value->provider_id != provider_id ||
                result.value->state_schema_version !=
                    ports->at(provider_id)->state_schema_version()) {
                return std::nullopt;
            }
            bundle.fragments.push_back(*result.value);
        }
        std::sort(bundle.fragments.begin(), bundle.fragments.end(),
                  [](const auto& left, const auto& right) {
                      return left.provider_id < right.provider_id;
                  });
        if (std::chrono::steady_clock::now() > deadline || !bundle.valid()) {
            return std::nullopt;
        }
        return bundle;
    }

    bool restore(const contracts::CognitiveStateBundleV1& bundle,
                 const std::string& correlation_id) {
        if (!bundle.valid()) return false;
        const auto required = required_provider_ids();
        if (!required || *required != bundle.required_provider_ids) return false;
        const auto ports = state_ports(*required);
        if (!ports) return false;

        const auto deadline = std::chrono::steady_clock::now() + capture_budget_;
        const auto coordinator_baseline = coordinator_.capture_checkpoint();
        std::map<std::string, contracts::CognitiveStateFragmentV1> baselines;
        for (const auto& provider_id : *required) {
            contracts::PortInvocationContextV1 context;
            context.correlation_id = correlation_id;
            context.deadline = deadline;
            const auto captured = ports->at(provider_id)->capture_state(context);
            if (!captured.valid() || !captured.success || !captured.value ||
                !captured.value->valid()) {
                return false;
            }
            baselines.emplace(provider_id, *captured.value);
        }

        std::vector<std::string> mutated;
        for (const auto& fragment : bundle.fragments) {
            contracts::PortInvocationContextV1 context;
            context.correlation_id = correlation_id;
            context.deadline = deadline;
            const auto restored =
                ports->at(fragment.provider_id)->restore_state(fragment, context);
            if (!restored.valid() || !restored.success || !restored.value ||
                !restored.value->valid() ||
                restored.value->provider_id != fragment.provider_id) {
                rollback(*ports, baselines, mutated, coordinator_baseline,
                         correlation_id);
                return false;
            }
            mutated.push_back(fragment.provider_id);
        }
        if (!coordinator_.restore_checkpoint(bundle.coordinator)) {
            rollback(*ports, baselines, mutated, coordinator_baseline,
                     correlation_id);
            return false;
        }
        return true;
    }

    std::optional<std::vector<std::string>> required_provider_ids() const {
        std::vector<std::string> required;
        for (const auto operation : kStatefulCognitiveOperations) {
            try {
                const auto resolution = registry_.resolve(std::string(operation));
                const auto& record = registry_.record(resolution.implementation_id);
                if (!record.descriptor.supports_checkpoint) return std::nullopt;
                required.push_back(resolution.implementation_id);
            } catch (const NoCapabilityProvider&) {
                // An absent optional cognitive capability has no state to capture.
            }
        }
        std::sort(required.begin(), required.end());
        required.erase(std::unique(required.begin(), required.end()),
                       required.end());
        return required;
    }

private:
    using StatePorts =
        std::map<std::string, std::shared_ptr<ICognitiveStatePort>>;

    std::optional<StatePorts> state_ports(
        const std::vector<std::string>& required) const {
        StatePorts ports;
        for (const auto& port :
             registry_.resolve_all<ICognitiveStatePort>("cognitive_state")) {
            if (!port || port->provider_id().empty() ||
                ports.contains(port->provider_id())) {
                return std::nullopt;
            }
            ports.emplace(port->provider_id(), port);
        }
        if (ports.size() != required.size()) return std::nullopt;
        for (const auto& provider_id : required) {
            if (!ports.contains(provider_id)) return std::nullopt;
        }
        return ports;
    }

    void rollback(
        const StatePorts& ports,
        const std::map<std::string, contracts::CognitiveStateFragmentV1>& baselines,
        const std::vector<std::string>& mutated,
        const contracts::CognitiveCoordinatorCheckpointV1& coordinator_baseline,
        const std::string& correlation_id) {
        for (auto item = mutated.rbegin(); item != mutated.rend(); ++item) {
            contracts::PortInvocationContextV1 context;
            context.correlation_id = correlation_id;
            context.deadline = std::chrono::steady_clock::now() + capture_budget_;
            (void)ports.at(*item)->restore_state(baselines.at(*item), context);
        }
        (void)coordinator_.restore_checkpoint(coordinator_baseline);
    }

    const CapabilityRegistry& registry_;
    CognitiveCoordinator& coordinator_;
    std::chrono::milliseconds capture_budget_;
};

}  // namespace eu_digital
