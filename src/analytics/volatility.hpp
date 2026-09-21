#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <vector>

namespace pql {

struct Volatility {
    double per_period;  // Fractional sample standard deviation, not percentage points.
    std::optional<double> annualized;
    std::size_t observations;
    bool operator==(const Volatility&) const = default;
};

class VolatilityError : public std::runtime_error {
   public:
    using std::runtime_error::runtime_error;
};

// Equally spaced fractional simple returns (0.01 = 1%), finite and >= -1.
// Uses n-1; fewer than two observations returns null after validating all input.
// Optional annualization is sigma * sqrt(periods_per_year), an explicit modeling
// assumption, not ACT/365 geometric return annualization or a forecast.
// No missing-value substitution, input mutation, or inferred sampling frequency.
[[nodiscard]] std::optional<Volatility> sampleVolatility(
    const std::vector<double>& fractional_returns,
    std::optional<std::uint32_t> periods_per_year = std::nullopt);

}  // namespace pql
