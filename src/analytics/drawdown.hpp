#pragma once

#include <optional>
#include <stdexcept>
#include <vector>

#include "domain/financial_types.hpp"

namespace pql {

struct EquityPoint {
    Timestamp timestamp;
    Money value;
    bool operator==(const EquityPoint&) const = default;
};

struct MaximumDrawdown {
    double fraction;  // [-1, 0]; -0.25 means a 25% decline from the prior peak.
    EquityPoint peak;
    EquityPoint trough;
    bool operator==(const MaximumDrawdown&) const = default;
};

class DrawdownError : public std::runtime_error {
   public:
    using std::runtime_error::runtime_error;
};

// Single portfolio's comparable equity observations, with no unadjusted external
// cash flows. Times must be strictly increasing; inputs are never sorted/mutated.
// Empty/all-zero curves have no defined peak denominator. Leading zeros are skipped.
// No decline: zero with both endpoints at the first positive observation.
// Ties retain the earliest peak and first equally deep trough. Results own points.
// Measures observed drawdown only; daily samples do not reveal intraday troughs.
[[nodiscard]] std::optional<MaximumDrawdown> maximumDrawdown(const std::vector<EquityPoint>& curve);

}  // namespace pql
