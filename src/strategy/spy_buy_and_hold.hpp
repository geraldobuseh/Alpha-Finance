#pragma once

#include "execution/trading_costs.hpp"
#include "strategy/strategy.hpp"

namespace pql {

// Dedicated benchmark: proposes one fractional SPY buy on a pristine account.
// Accepted ledger history, not a mutable strategy flag, determines whether to hold.
class SpyBuyAndHold final : public Strategy {
   public:
    SpyBuyAndHold(OrderId initial_order, TradingCosts costs)
        : initial_order_(initial_order), costs_(costs) {}
    [[nodiscard]] std::string name() const override { return "SPY Buy-and-Hold"; }
    [[nodiscard]] std::vector<Order> generateOrders(const MarketState& market,
                                                    const PortfolioState& portfolio) override;

   private:
    OrderId initial_order_;
    TradingCosts costs_;
};

}  // namespace pql
