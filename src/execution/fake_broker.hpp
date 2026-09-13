#pragma once

#include "domain/portfolio.hpp"
#include "execution/broker.hpp"
#include "execution/execution.hpp"
#include "execution/market_state.hpp"
#include "execution/trading_costs.hpp"

namespace pql {

// Trusted, sequential execution authority. Portfolio must outlive the broker.
// Strategies receive a PortfolioSnapshot, never this portfolio reference.
class FakeBroker final : public Broker {
   public:
    // Compatibility: fixed commission per full fill, zero slippage.
    FakeBroker(Portfolio& portfolio, Money fee);
    FakeBroker(Portfolio& portfolio, TradingCosts costs);
    [[nodiscard]] Execution execute(const Order& order, const MarketState& market) override;

   private:
    Portfolio& portfolio_;
    TradingCosts costs_;
};

}  // namespace pql
