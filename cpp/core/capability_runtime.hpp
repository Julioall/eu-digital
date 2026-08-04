#pragma once

#include <algorithm>
#include <functional>
#include <fstream>
#include <iomanip>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include <iostream>

namespace eu_digital {

enum class CapabilityState {
    unknown,
    discovered,
    calibrating,
    available,
    degraded,
    temporarily_unavailable,
    disabled,
    failed,
    removed,
    incompatible,
};

struct CapabilityOperation {
    std::string operation;
    std::string output_schema;
};

struct CapabilityDescriptor {
    std::string schema_version{"1.0"};
    std::string capability_id;
    std::string implementation_id;
    std::string implementation_version;
    std::string kind;
    std::vector<CapabilityOperation> provides;
    std::vector<std::string> mandatory_operations;
    bool supports_hot_plug{false};
    bool supports_checkpoint{false};
    std::vector<std::string> permissions;

    bool valid() const {
        return schema_version == "1.0" && !capability_id.empty() && !implementation_id.empty() &&
            !implementation_version.empty() && !kind.empty();
    }

    bool provides_operation(const std::string& operation) const {
        return std::any_of(provides.begin(), provides.end(), [&](const CapabilityOperation& item) {
            return item.operation == operation;
        });
    }
};

struct CapabilityStateRecord {
    CapabilityState state{CapabilityState::unknown};
    std::string reason_code;
    std::string message;
    std::vector<std::string> blocked_goal_ids;
};

struct CapabilityRecord {
    CapabilityDescriptor descriptor;
    CapabilityStateRecord state;
    int priority{0};
    std::map<std::string, std::string> checkpoint;
    std::shared_ptr<void> instance;
};

struct SelfModelState {
    std::size_t version{0};
    std::vector<std::string> available;
    std::vector<std::string> degraded;
    std::vector<std::string> temporarily_unavailable;
    std::vector<std::string> disabled;
    std::vector<std::string> removed;
    std::vector<std::string> capability_history;
    std::vector<std::string> active_goal_ids;
};

struct Resolution {
    std::string operation;
    std::string implementation_id;
    bool fallback{false};
    std::string reason;
};

class NoCapabilityProvider : public std::runtime_error {
public:
    explicit NoCapabilityProvider(const std::string& operation)
        : std::runtime_error("no provider for operation: " + operation) {}
};

class CapabilityLifecycleError : public std::runtime_error {
public:
    explicit CapabilityLifecycleError(const std::string& message) : std::runtime_error(message) {}
};

class CapabilityRegistry {
public:
    using EventSink = std::function<void(const std::string&, const std::string&)>;

    explicit CapabilityRegistry(EventSink event_sink = {}) : event_sink_(std::move(event_sink)) {}

    void discover(CapabilityDescriptor descriptor, int priority = 0) {
        if (!descriptor.valid()) throw CapabilityLifecycleError("invalid CapabilityDescriptor");
        auto found = records_.find(descriptor.implementation_id);
        if (found != records_.end() && found->second.state.state != CapabilityState::removed) {
            throw CapabilityLifecycleError("implementation already registered: " + descriptor.implementation_id);
        }
        CapabilityRecord record{std::move(descriptor), {}, priority, {}};
        if (found != records_.end()) record.checkpoint = std::move(found->second.checkpoint);
        const auto implementation_id = record.descriptor.implementation_id;
        records_[implementation_id] = std::move(record);
        refresh_self_model();
    }

    void transition(const std::string& implementation_id, CapabilityState next,
                    std::string reason_code = {}, std::string message = {}) {
        auto& record = records_.at(implementation_id);
        if (!allowed(record.state.state, next)) {
            throw CapabilityLifecycleError("invalid capability state transition");
        }
        record.state.state = next;
        record.state.reason_code = std::move(reason_code);
        record.state.message = std::move(message);
        emit_event("capability." + state_name(next), record);
        refresh_self_model();
    }

    void set_checkpoint(const std::string& implementation_id, std::map<std::string, std::string> checkpoint) {
        records_.at(implementation_id).checkpoint = std::move(checkpoint);
    }

    void register_plan(std::string plan_id, std::set<std::string> required_implementations) {
        plans_[std::move(plan_id)] = std::move(required_implementations);
    }

    void invalidate_for(const std::string& implementation_id) {
        for (const auto& [plan_id, requirements] : plans_) {
            if (requirements.contains(implementation_id)) blocked_plans_.insert(plan_id);
        }
        refresh_self_model();
    }

    void define_profile(std::string profile_id, std::set<std::string> implementations) {
        profiles_[std::move(profile_id)] = std::move(implementations);
    }

    void activate_profile(std::optional<std::string> profile_id) {
        if (profile_id && !profiles_.contains(*profile_id)) throw CapabilityLifecycleError("unknown capability profile");
        active_profile_ = std::move(profile_id);
    }

    const CapabilityRecord& record(const std::string& implementation_id) const { return records_.at(implementation_id); }

    const std::map<std::string, CapabilityRecord>& records() const { return records_; }

    const SelfModelState& self_model() const { return self_model_; }

    std::optional<std::string> active_profile() const { return active_profile_; }

    void save(const std::string& path) const {
        std::ofstream output(path, std::ios::trunc);
        if (!output) throw CapabilityLifecycleError("cannot persist capability registry: " + path);
        output << "EU_DIGITAL_CAPABILITY_REGISTRY 1\n";
        for (const auto& [implementation_id, record] : records_) {
            output << "record " << std::quoted(implementation_id) << ' '
                   << std::quoted(record.descriptor.capability_id) << ' '
                   << std::quoted(record.descriptor.implementation_version) << ' '
                   << std::quoted(record.descriptor.kind) << ' '
                   << static_cast<int>(record.state.state) << ' ' << record.priority << ' '
                   << std::quoted(record.state.reason_code) << ' ' << std::quoted(record.state.message) << '\n';
            output << "flags " << std::quoted(implementation_id) << ' '
                   << record.descriptor.supports_hot_plug << ' ' << record.descriptor.supports_checkpoint << '\n';
            for (const auto& operation : record.descriptor.provides) {
                output << "provide " << std::quoted(implementation_id) << ' '
                       << std::quoted(operation.operation) << ' ' << std::quoted(operation.output_schema) << '\n';
            }
            for (const auto& operation : record.descriptor.mandatory_operations) {
                output << "mandatory " << std::quoted(implementation_id) << ' ' << std::quoted(operation) << '\n';
            }
            for (const auto& permission : record.descriptor.permissions) {
                output << "permission " << std::quoted(implementation_id) << ' ' << std::quoted(permission) << '\n';
            }
            for (const auto& [key, value] : record.checkpoint) {
                output << "checkpoint " << std::quoted(implementation_id) << ' '
                       << std::quoted(key) << ' ' << std::quoted(value) << '\n';
            }
        }
        for (const auto& [plan_id, requirements] : plans_) {
            output << "plan " << std::quoted(plan_id) << ' ' << requirements.size();
            for (const auto& requirement : requirements) output << ' ' << std::quoted(requirement);
            output << '\n';
        }
        for (const auto& [profile_id, implementations] : profiles_) {
            output << "profile " << std::quoted(profile_id) << ' ' << implementations.size();
            for (const auto& implementation : implementations) output << ' ' << std::quoted(implementation);
            output << '\n';
        }
        output << "blocked";
        for (const auto& plan_id : blocked_plans_) output << ' ' << std::quoted(plan_id);
        output << '\n';
        output << "active " << std::quoted(active_profile_.value_or("")) << '\n';
        output << "end\n";
    }

    static CapabilityRegistry load(const std::string& path, EventSink event_sink = {}) {
        std::ifstream input(path);
        if (!input) throw CapabilityLifecycleError("cannot restore capability registry: " + path);
        std::string header;
        int version = 0;
        if (!(input >> header >> version) || header != "EU_DIGITAL_CAPABILITY_REGISTRY" || version != 1) {
            throw CapabilityLifecycleError("unsupported capability registry snapshot");
        }
        CapabilityRegistry registry(std::move(event_sink));
        std::string tag;
        while (input >> tag && tag != "end") {
            if (tag == "record") {
                std::string implementation_id, capability_id, implementation_version, kind, reason, message;
                int state = 0;
                int priority = 0;
                input >> std::quoted(implementation_id) >> std::quoted(capability_id) >>
                    std::quoted(implementation_version) >> std::quoted(kind) >> state >> priority >>
                    std::quoted(reason) >> std::quoted(message);
                CapabilityDescriptor descriptor;
                descriptor.capability_id = std::move(capability_id);
                descriptor.implementation_id = implementation_id;
                descriptor.implementation_version = std::move(implementation_version);
                descriptor.kind = std::move(kind);
                CapabilityStateRecord state_record;
                state_record.state = state_from_int(state);
                state_record.reason_code = std::move(reason);
                state_record.message = std::move(message);
                registry.records_[implementation_id] = {std::move(descriptor), std::move(state_record), priority, {}};
            } else if (tag == "flags") {
                std::string implementation_id;
                bool hot_plug = false;
                bool checkpoint = false;
                input >> std::quoted(implementation_id) >> hot_plug >> checkpoint;
                auto& descriptor = registry.records_.at(implementation_id).descriptor;
                descriptor.supports_hot_plug = hot_plug;
                descriptor.supports_checkpoint = checkpoint;
            } else if (tag == "provide") {
                std::string implementation_id, operation, output_schema;
                input >> std::quoted(implementation_id) >> std::quoted(operation) >> std::quoted(output_schema);
                registry.records_.at(implementation_id).descriptor.provides.push_back({std::move(operation), std::move(output_schema)});
            } else if (tag == "mandatory") {
                std::string implementation_id, operation;
                input >> std::quoted(implementation_id) >> std::quoted(operation);
                registry.records_.at(implementation_id).descriptor.mandatory_operations.push_back(std::move(operation));
            } else if (tag == "permission") {
                std::string implementation_id, permission;
                input >> std::quoted(implementation_id) >> std::quoted(permission);
                registry.records_.at(implementation_id).descriptor.permissions.push_back(std::move(permission));
            } else if (tag == "checkpoint") {
                std::string implementation_id, key, value;
                input >> std::quoted(implementation_id) >> std::quoted(key) >> std::quoted(value);
                registry.records_.at(implementation_id).checkpoint.emplace(std::move(key), std::move(value));
            } else if (tag == "plan") {
                std::string plan_id;
                std::size_t count = 0;
                input >> std::quoted(plan_id) >> count;
                auto& requirements = registry.plans_[plan_id];
                for (std::size_t index = 0; index < count; ++index) {
                    std::string requirement;
                    input >> std::quoted(requirement);
                    requirements.insert(std::move(requirement));
                }
            } else if (tag == "profile") {
                std::string profile_id;
                std::size_t count = 0;
                input >> std::quoted(profile_id) >> count;
                auto& implementations = registry.profiles_[profile_id];
                for (std::size_t index = 0; index < count; ++index) {
                    std::string implementation;
                    input >> std::quoted(implementation);
                    implementations.insert(std::move(implementation));
                }
            } else if (tag == "blocked") {
                std::string plan_id;
                std::getline(input, plan_id);
                std::istringstream values(plan_id);
                while (values >> std::quoted(plan_id)) registry.blocked_plans_.insert(plan_id);
            } else if (tag == "active") {
                std::string profile_id;
                input >> std::quoted(profile_id);
                if (!profile_id.empty()) registry.active_profile_ = std::move(profile_id);
            } else {
                throw CapabilityLifecycleError("invalid capability registry record: " + tag);
            }
        }
        registry.refresh_self_model();
        return registry;
    }

    Resolution resolve(const std::string& operation, std::optional<std::string> preferred = std::nullopt) const {
        std::vector<const CapabilityRecord*> candidates;
        for (const auto& [implementation_id, record] : records_) {
            if (!allowed_by_profile(implementation_id) ||
                (record.state.state != CapabilityState::available && record.state.state != CapabilityState::degraded) ||
                !record.descriptor.provides_operation(operation)) continue;
            candidates.push_back(&record);
        }
        if (candidates.empty()) throw NoCapabilityProvider(operation);
        const CapabilityRecord* selected = nullptr;
        if (preferred) {
            auto item = std::find_if(candidates.begin(), candidates.end(), [&](const CapabilityRecord* record) {
                return record->descriptor.implementation_id == *preferred;
            });
            if (item != candidates.end()) selected = *item;
        }
        if (selected == nullptr) {
            selected = *std::max_element(candidates.begin(), candidates.end(), [](const auto* left, const auto* right) {
                return left->priority < right->priority;
            });
        }
        const bool fallback = preferred && selected->descriptor.implementation_id != *preferred;
        Resolution result{operation, selected->descriptor.implementation_id, fallback,
                           fallback ? "fallback_provider" : (preferred ? "preferred_provider" : "highest_priority")};
        if (event_sink_) event_sink_("capability.resolved", result.implementation_id);
        return result;
    }

    template <typename T>
    void register_instance(const std::string& operation, std::shared_ptr<T> instance) {
        CapabilityDescriptor descriptor;
        descriptor.capability_id = operation;
        descriptor.implementation_id = operation + "_impl";
        descriptor.implementation_version = "1.0.0";
        descriptor.kind = "mock";
        descriptor.provides.push_back({operation, "{}"});
        register_instance(std::move(descriptor), std::move(instance), 100);
    }

    template <typename T>
    void register_instance(CapabilityDescriptor descriptor, std::shared_ptr<T> instance,
                           int priority = 0) {
        if (!instance) {
            throw CapabilityLifecycleError("cannot register a null capability instance");
        }
        if (descriptor.provides.empty()) {
            throw CapabilityLifecycleError("capability instance must provide an operation");
        }

        const auto implementation_id = descriptor.implementation_id;
        discover(std::move(descriptor), priority);
        records_.at(implementation_id).instance = std::move(instance);
        transition(implementation_id, CapabilityState::discovered);
        transition(implementation_id, CapabilityState::calibrating);
        transition(implementation_id, CapabilityState::available);
    }

    template <typename T>
    std::shared_ptr<T> resolve(const std::string& operation) const {
        try {
            auto resolution = resolve(operation, std::nullopt); // explicitly call non-template
            auto found = records_.find(resolution.implementation_id);
            if (found != records_.end() && found->second.instance) {
                return std::static_pointer_cast<T>(found->second.instance);
            }
        } catch (const NoCapabilityProvider&) {
            // Not found
        }
        return nullptr;
    }

private:
    static bool allowed(CapabilityState current, CapabilityState next) {
        if (current == next) return true;
        switch (current) {
        case CapabilityState::unknown: return next == CapabilityState::discovered || next == CapabilityState::incompatible || next == CapabilityState::removed;
        case CapabilityState::discovered: return next == CapabilityState::calibrating || next == CapabilityState::incompatible || next == CapabilityState::failed || next == CapabilityState::removed;
        case CapabilityState::calibrating: return next == CapabilityState::available || next == CapabilityState::failed || next == CapabilityState::temporarily_unavailable || next == CapabilityState::removed;
        case CapabilityState::available: return next == CapabilityState::degraded || next == CapabilityState::temporarily_unavailable || next == CapabilityState::disabled || next == CapabilityState::failed || next == CapabilityState::removed;
        case CapabilityState::degraded: return next == CapabilityState::available || next == CapabilityState::temporarily_unavailable || next == CapabilityState::disabled || next == CapabilityState::failed || next == CapabilityState::removed;
        case CapabilityState::temporarily_unavailable: return next == CapabilityState::calibrating || next == CapabilityState::available || next == CapabilityState::disabled || next == CapabilityState::failed || next == CapabilityState::removed;
        case CapabilityState::disabled: return next == CapabilityState::calibrating || next == CapabilityState::available || next == CapabilityState::removed;
        case CapabilityState::failed: return next == CapabilityState::calibrating || next == CapabilityState::temporarily_unavailable || next == CapabilityState::removed;
        case CapabilityState::removed: return next == CapabilityState::discovered;
        case CapabilityState::incompatible: return next == CapabilityState::discovered || next == CapabilityState::removed;
        }
        return false;
    }

    static std::string state_name(CapabilityState state) {
        switch (state) {
        case CapabilityState::unknown: return "unknown";
        case CapabilityState::discovered: return "discovered";
        case CapabilityState::calibrating: return "calibrating";
        case CapabilityState::available: return "available";
        case CapabilityState::degraded: return "degraded";
        case CapabilityState::temporarily_unavailable: return "temporarily_unavailable";
        case CapabilityState::disabled: return "disabled";
        case CapabilityState::failed: return "failed";
        case CapabilityState::removed: return "removed";
        case CapabilityState::incompatible: return "incompatible";
        }
        return "unknown";
    }

    static CapabilityState state_from_int(int value) {
        if (value < static_cast<int>(CapabilityState::unknown) || value > static_cast<int>(CapabilityState::incompatible)) {
            throw CapabilityLifecycleError("invalid persisted capability state");
        }
        return static_cast<CapabilityState>(value);
    }

    bool allowed_by_profile(const std::string& implementation_id) const {
        return !active_profile_ || profiles_.at(*active_profile_).contains(implementation_id);
    }

    void emit_event(const std::string& event_type, const CapabilityRecord& record) const {
        if (event_sink_) event_sink_(event_type, record.descriptor.implementation_id);
    }

    void refresh_self_model() {
        ++self_model_.version;
        self_model_.available.clear();
        self_model_.degraded.clear();
        self_model_.temporarily_unavailable.clear();
        self_model_.disabled.clear();
        self_model_.removed.clear();
        for (const auto& [implementation_id, record] : records_) {
            self_model_.capability_history.push_back(implementation_id);
            switch (record.state.state) {
            case CapabilityState::available: self_model_.available.push_back(implementation_id); break;
            case CapabilityState::degraded: self_model_.degraded.push_back(implementation_id); break;
            case CapabilityState::temporarily_unavailable: self_model_.temporarily_unavailable.push_back(implementation_id); break;
            case CapabilityState::disabled: self_model_.disabled.push_back(implementation_id); break;
            case CapabilityState::removed: self_model_.removed.push_back(implementation_id); break;
            default: break;
            }
        }
        std::sort(self_model_.capability_history.begin(), self_model_.capability_history.end());
        self_model_.capability_history.erase(std::unique(self_model_.capability_history.begin(), self_model_.capability_history.end()), self_model_.capability_history.end());
    }

    EventSink event_sink_;
    std::map<std::string, CapabilityRecord> records_;
    std::map<std::string, std::set<std::string>> plans_;
    std::set<std::string> blocked_plans_;
    std::map<std::string, std::set<std::string>> profiles_;
    std::optional<std::string> active_profile_;
    SelfModelState self_model_;
};

class CapabilityPlugin {
public:
    virtual ~CapabilityPlugin() = default;
    virtual const CapabilityDescriptor& descriptor() const = 0;
    virtual void validate_manifest() = 0;
    virtual void configure() = 0;
    virtual void initialize() = 0;
    virtual void calibrate() = 0;
    virtual bool health_check() = 0;
    virtual void start() = 0;
    virtual void drain() = 0;
    virtual std::map<std::string, std::string> checkpoint() = 0;
    virtual void stop() = 0;
    virtual void uninstall() = 0;
};

class ModuleLifecycleManager {
public:
    explicit ModuleLifecycleManager(CapabilityRegistry& registry) : registry_(registry) {}

    bool install(CapabilityPlugin& plugin, int priority = 0) {
        const auto& descriptor = plugin.descriptor();
        plugins_[descriptor.implementation_id] = &plugin;
        registry_.discover(descriptor, priority);
        try {
            plugin.validate_manifest();
            registry_.transition(descriptor.implementation_id, CapabilityState::discovered);
            for (const auto& operation : descriptor.mandatory_operations) {
                const bool satisfied = std::any_of(registry_.records().begin(), registry_.records().end(), [&](const auto& item) {
                    return item.first != descriptor.implementation_id &&
                        (item.second.state.state == CapabilityState::available || item.second.state.state == CapabilityState::degraded) &&
                        item.second.descriptor.provides_operation(operation);
                });
                if (!satisfied) throw CapabilityLifecycleError("missing mandatory dependency: " + operation);
            }
            registry_.transition(descriptor.implementation_id, CapabilityState::calibrating);
            plugin.configure();
            plugin.initialize();
            plugin.calibrate();
            if (!plugin.health_check()) throw CapabilityLifecycleError("health check failed");
            registry_.transition(descriptor.implementation_id, CapabilityState::available);
            plugin.start();
            return true;
        } catch (const std::exception& error) {
            registry_.transition(descriptor.implementation_id, CapabilityState::failed, "plugin_initialization_failed", error.what());
            return false;
        }
    }

    void remove(const std::string& implementation_id) {
        const auto& record = registry_.record(implementation_id);
        if (plugins_.contains(implementation_id)) {
            auto& plugin = *plugins_.at(implementation_id);
            plugin.drain();
            if (record.descriptor.supports_checkpoint) registry_.set_checkpoint(implementation_id, plugin.checkpoint());
            plugin.stop();
            plugin.uninstall();
        }
        registry_.invalidate_for(implementation_id);
        registry_.transition(implementation_id, CapabilityState::removed, "removed");
    }

    void attach(std::string implementation_id, CapabilityPlugin& plugin) {
        plugins_[std::move(implementation_id)] = &plugin;
    }

private:
    CapabilityRegistry& registry_;
    std::map<std::string, CapabilityPlugin*> plugins_;
};

}  // namespace eu_digital
