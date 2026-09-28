#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <vector>

namespace pql {

struct Sharpe {
    double per_period;  // Dimensionless, signed; arithmetic mean / sample deviation.
    std::optional<double> annualized;
    std::size_t observations;
    bool operator==(const Sharpe&) const = default;
};

class SharpeError : public std::runtime_error {
   public:
    using std::runtime_error::runtime_error;
};

// Simplified Sharpe-style ratio with a fixed ZERO per-period risk-free return.
// Finite fractional simple returns >= -1 over comparable equally spaced periods.
// Sample dispersion uses n-1. Fewer than two observations or zero dispersion is
// undefined (null), including constant gains/losses. Invalid inputs still reject.
// Optional annualization multiplies by sqrt(periods_per_year), never inferred.
[[nodiscard]] std::optional<Sharpe> sampleSharpe(
    const std::vector<double>& fractional_returns,
    std::optional<std::uint32_t> periods_per_year = std::nullopt);

}  // namespace pql
