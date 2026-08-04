#pragma once

#include <exception>
#include <optional>
#include <string>
#include <utility>

namespace eu_digital::contracts {

inline constexpr char kPortResultSchemaVersion[] = "1.0";

struct PortError {
    std::string schema_version{kPortResultSchemaVersion};
    std::string operation;
    std::string code;
    std::string message;
    bool retryable{false};

    bool valid() const {
        return schema_version == kPortResultSchemaVersion && !operation.empty() &&
               !code.empty();
    }
};

template <typename T>
struct PortResult {
    std::string schema_version{kPortResultSchemaVersion};
    bool success{false};
    std::optional<T> value;
    std::optional<PortError> error;

    static PortResult ok(T result) {
        PortResult envelope;
        envelope.success = true;
        envelope.value = std::move(result);
        return envelope;
    }

    static PortResult failed(std::string operation, std::string code,
                             std::string message, bool retryable = false) {
        PortResult envelope;
        envelope.error = PortError{kPortResultSchemaVersion, std::move(operation),
                                   std::move(code), std::move(message), retryable};
        return envelope;
    }

    bool valid() const {
        if (schema_version != kPortResultSchemaVersion) {
            return false;
        }
        if (success) {
            return value.has_value() && !error.has_value();
        }
        return !value.has_value() && error.has_value() && error->valid();
    }
};

template <typename T, typename Callable>
PortResult<T> capture_port_result(std::string operation, Callable&& callable) {
    try {
        return PortResult<T>::ok(std::forward<Callable>(callable)());
    } catch (const std::exception& error) {
        return PortResult<T>::failed(std::move(operation), "adapter_delegation_error",
                                     error.what());
    } catch (...) {
        return PortResult<T>::failed(std::move(operation),
                                     "unknown_adapter_delegation_error",
                                     "non-standard exception");
    }
}

}  // namespace eu_digital::contracts
