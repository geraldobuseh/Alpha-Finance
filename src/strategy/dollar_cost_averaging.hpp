#pragma once

#include "execution/trading_costs.hpp"
#include "strategy/strategy.hpp"

namespace pql {

// $25 total cash budget for SPY on each supplied UTC Monday. No cash deposits.
// Requires sequential execution with fresh snapshots and no pending ID reservations.
// Execute immediately with the same quote and costs: $25 is a sizing budget,
// not a market-order spending cap when execution conditions change.
class DollarCostAveraging final : public Strategy {
   public:
    explicit DollarCostAveraging(TradingCosts costs) : costs_(costs) {}
    [[nodiscard]] std::string name() const override { return "SPY Weekly Dollar-Cost Averaging"; }
    [[nodiscard]] std::vector<Order> generateOrders(const MarketState& market,
                                                    const PortfolioState& portfolio) override;

   private:
    TradingCosts costs_;
};

}  // namespace pql
