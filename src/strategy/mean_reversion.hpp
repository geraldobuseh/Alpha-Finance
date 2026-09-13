#pragma once

#include <stdexcept>

#include "execution/trading_costs.hpp"
#include "strategy/strategy.hpp"

namespace pql {

class MeanReversionError : public std::runtime_error {
   public:
    using std::runtime_error::runtime_error;
};

// Hypothesis: deviations below a trailing average may reverse, not a guarantee.
// Single-symbol, long-only: all available cash on entry, full exit, no pyramiding.
// MA uses the latest 20 completed session closes, excluding the current quote.
// Caller supplies complete, consistently adjusted session history and fresh quotes.
// Execute sequentially with fresh portfolio snapshots, no pending reservations,
// and the same quotes/costs used for sizing (market orders have no spending cap).
class MeanReversion final : public Strategy {
   public:
    MeanReversion(Symbol symbol, TradingCosts costs) : symbol_(symbol), costs_(costs) {}
    [[nodiscard]] std::string name() const override { return "20-Day Mean Reversion"; }
    [[nodiscard]] std::vector<Order> generateOrders(const MarketState& market,
                                                    const PortfolioState& portfolio) override;

   private:
    Symbol symbol_;
    TradingCosts costs_;
};

}  // namespace pql
