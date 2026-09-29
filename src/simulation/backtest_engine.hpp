#pragma once

#include <stdexcept>
#include <string>
#include <vector>

#include "analytics/daily_valuation.hpp"
#include "domain/portfolio.hpp"
#include "execution/execution.hpp"
#include "execution/market_state.hpp"
#include "execution/trading_costs.hpp"
#include "strategy/strategy.hpp"

namespace pql {

class BacktestError : public std::runtime_error {
   public:
    using std::runtime_error::runtime_error;
};

// Idealized close execution: quotes and valuation marks share the same raw close.
// MarketState exposes only prior-session history plus the current decision quote.
struct HistoricalTradingDay {
    Date session;
    Timestamp close;
    MarketState market;
    std::vector<PriceBar> closing_prices;
};
using BacktestTradingDay = HistoricalTradingDay;

class HistoricalMarketData {
   public:
    explicit HistoricalMarketData(std::vector<HistoricalTradingDay> days);

    [[nodiscard]] const std::vector<HistoricalTradingDay>& tradingDays() const noexcept {
        return days_;
    }

   private:
    std::vector<HistoricalTradingDay> days_;
};

struct BacktestDailySnapshot {
    Date session;
    Timestamp as_of;
    PortfolioSnapshot portfolio;
    std::vector<Execution> executions;
    DailyValuation valuation;
};

struct BacktestResult {
    std::string strategy_name;
    PortfolioSnapshot initial_portfolio;
    PortfolioSnapshot final_portfolio;
    std::vector<BacktestDailySnapshot> snapshots;
};

class BacktestEngine {
   public:
    explicit BacktestEngine(TradingCosts costs = TradingCosts{Money::create(0.0).value(), 0.0});

    // Repeatability requires identical inputs and equivalent initial strategy state.
    // Data and the caller's portfolio remain unchanged. The strategy may mutate its
    // own state, including on failure; callers must reset it before retrying.
    // Invalid data or rejected orders abort the run without returning partial results.
    [[nodiscard]] BacktestResult run(Strategy& strategy, HistoricalMarketData& data,
                                     Portfolio initialPortfolio) const;

   private:
    TradingCosts costs_;
};

[[nodiscard]] BacktestResult run(Strategy& strategy, HistoricalMarketData& data,
                                 Portfolio initialPortfolio);

}  // namespace pql
