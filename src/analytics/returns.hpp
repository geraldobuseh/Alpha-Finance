#pragma once

#include <chrono>
#include <optional>
#include <stdexcept>

#include "domain/financial_types.hpp"

namespace pql {

class ReturnError : public std::runtime_error {
   public:
    using std::runtime_error::runtime_error;
};

// Fractional simple returns for nonnegative portfolio values, with no external
// deposits/withdrawals. Null means a zero denominator, not a zero return.
// Invalid inputs and unrepresentable results throw ReturnError.
[[nodiscard]] std::optional<double> dailyReturn(Money current, Money previous);
[[nodiscard]] std::optional<double> cumulativeReturn(Money current, Money starting);

// Excess return over SPY, not risk-adjusted alpha or relative wealth growth.
// Finite fractional simple-return inputs >= -1 must cover matching periods and
// comparable fee, currency, funding and dividend/adjustment conventions.
// Output is strategy_return - spy_return; it may legitimately be below -1.
// Missing observations must not be substituted with zero. Throws ReturnError
// for invalid inputs or a subtraction that completely loses a nonzero operand.
[[nodiscard]] double excess_return(double strategy_return, double spy_return);

// Geometric annual equivalent using ACT/365 Fixed: actual elapsed calendar days
// divided by 365 (including leap-day observations in elapsed). Not trading days.
// Caller supplies positive duration from the starting-value observation, not from
// the first post-trade snapshot. Annualization is a normalization, not a forecast.
[[nodiscard]] std::optional<double> annualizedReturn(Money current, Money starting,
                                                     std::chrono::days elapsed);

}  // namespace pql
