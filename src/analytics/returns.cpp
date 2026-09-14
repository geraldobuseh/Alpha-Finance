#include "analytics/returns.hpp"

#include <cmath>
#include <limits>

namespace pql {
namespace {
void validateValues(Money current, Money base) {
    if (current.value() < 0.0 || base.value() < 0.0) {
        throw ReturnError("Portfolio values must be nonnegative");
    }
}

double checked(double result, Money current, Money base) {
    if (!std::isfinite(result) || result < -1.0 || (current != base && result == 0.0) ||
        (current.value() > 0.0 && result == -1.0)) {
        throw ReturnError("Unrepresentable portfolio return");
    }
    return result;
}

std::optional<double> simpleReturn(Money current, Money base) {
    validateValues(current, base);
    if (base.value() == 0.0) return std::nullopt;
    // Preserve PQL-024 operation order: persisted returns compare exactly on read.
    return checked((current.value() - base.value()) / base.value(), current, base);
}
}  // namespace

std::optional<double> dailyReturn(Money current, Money previous) {
    return simpleReturn(current, previous);
}

std::optional<double> cumulativeReturn(Money current, Money starting) {
    return simpleReturn(current, starting);
}

std::optional<double> annualizedReturn(Money current, Money starting, std::chrono::days elapsed) {
    validateValues(current, starting);
    if (elapsed.count() <= 0) throw ReturnError("Annualization requires positive elapsed days");
    if (starting.value() == 0.0) return std::nullopt;
    if (elapsed == std::chrono::days{365}) return simpleReturn(current, starting);
    if (current.value() == 0.0) return -1.0;
    if (current == starting) return 0.0;

    // Solve (1 + annual)^years = current/starting. log1p/expm1 retain small
    // changes; log differences avoid ratio overflow/underflow for distant values.
    const double change = (current.value() - starting.value()) / starting.value();
    // Near total loss, 1 + change can lose much of the remaining wealth. Only
    // use log1p near unchanged values; use the direct gross ratio farther away.
    double log_growth;
    if (std::abs(change) <= 0.5) {
        log_growth = std::log1p(change);
    } else {
        const double ratio = current.value() / starting.value();
        log_growth = std::isfinite(ratio) && ratio >= std::numeric_limits<double>::min()
                         ? std::log(ratio)
                         : std::log(current.value()) - std::log(starting.value());
    }
    const double exponent = log_growth * (365.0 / static_cast<double>(elapsed.count()));
    return checked(std::expm1(exponent), current, starting);
}

}  // namespace pql
