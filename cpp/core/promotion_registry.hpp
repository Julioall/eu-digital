#pragma once

#include <map>
#include <stdexcept>
#include <string>
#include <utility>

namespace eu_digital {

struct PromotionRecord {
    std::string component_id;
    std::string promotion_id;
    std::string component_version;
    std::string review_id;
};

class PromotionGateError : public std::runtime_error {
public:
    explicit PromotionGateError(const std::string& message) : std::runtime_error(message) {}
};

class PromotionRegistry {
public:
    void approve(PromotionRecord record) {
        if (record.component_id.empty() || record.promotion_id.empty() || record.review_id.empty()) {
            throw PromotionGateError("promotion record requires component, promotion and review");
        }
        records_[record.component_id] = std::move(record);
    }

    const PromotionRecord& require(const std::string& component_id) const {
        const auto found = records_.find(component_id);
        if (found == records_.end()) {
            throw PromotionGateError("component has no approved promotion: " + component_id);
        }
        return found->second;
    }

    std::string promotion_for(const std::string& component_id) const {
        return require(component_id).promotion_id;
    }

private:
    std::map<std::string, PromotionRecord> records_;
};

class PromotionFixtureRunner {
public:
    static std::string echo(const std::string& fixture_bytes) { return fixture_bytes; }
};

}  // namespace eu_digital
