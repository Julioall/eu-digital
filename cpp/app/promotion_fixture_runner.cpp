#include "core/promotion_registry.hpp"

#include <iostream>
#include <iterator>

int main() {
    const std::string fixture_bytes(
        std::istreambuf_iterator<char>(std::cin),
        std::istreambuf_iterator<char>());
    std::cout << eu_digital::PromotionFixtureRunner::echo(fixture_bytes);
    return std::cout.good() ? 0 : 1;
}
