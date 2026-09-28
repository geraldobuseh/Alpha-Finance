#pragma once

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "analytics/drawdown.hpp"
#include "analytics/sharpe.hpp"
#include "analytics/volatility.hpp"
#include "domain/portfolio.hpp"

namespace pql {

class ScorecardError : public std::runtime_error {
   public:
    using std::runtime_error::runtime_error;
};

struct StrategyScorecard {
    std::string strategy_name;
    Money starting_capital;
    Money ending_capital;
    double total_return;
    double spy_return;
    double excess_return;
    MaximumDrawdown max_drawdown;
    std::optional<Volatility> volatility;
    std::optional<Sharpe> sharpe;
    std::size_t number_of_trades;
    double turnover;
    Money fees;
    bool operator==(const StrategyScorecard&) const = default;
};

// Produces an inception-to-cutoff scorecard from observed equity and the terminal
// strategy ledger. Both curves must be nonempty, timestamp-aligned and strictly
// ordered. The strategy curve starts at immutable starting cash before any trade.
// Each adjacent interval should represent a comparable full reporting period when
// volatility or Sharpe are consumed; the builder cannot infer market calendars.
// Turnover is two-sided gross executed notional / mean sampled strategy equity;
// it is neither divided by two nor annualized. Fees are reported, not deducted
// again. Optional frequency only annualizes volatility and the simplified Sharpe.
[[nodiscard]] StrategyScorecard buildStrategyScorecard(
    std::string strategy_name, const PortfolioSnapshot& terminal_portfolio,
    const std::vector<EquityPoint>& strategy_equity, const std::vector<EquityPoint>& spy_equity,
    std::optional<std::uint32_t> periods_per_year = std::nullopt);

}  // namespace pql
