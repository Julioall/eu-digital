#include "core/promotion_registry.hpp"

#include <cassert>
#include <string>

int main() {
    eu_digital::PromotionRegistry registry;
    try {
        registry.promotion_for("missing.component");
        assert(false);
    } catch (const eu_digital::PromotionGateError&) {
    }

    registry.approve({"test.component", "promotion.test.v1", "1.0.0", "review-1"});
    assert(registry.promotion_for("test.component") == "promotion.test.v1");
    assert(eu_digital::PromotionFixtureRunner::echo("{\"value\":1}\n") == "{\"value\":1}\n");
}
