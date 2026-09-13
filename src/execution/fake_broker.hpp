#pragma once

#include "domain/portfolio.hpp"
#include "execution/broker.hpp"
#include "execution/execution.hpp"
#include "execution/market_state.hpp"

namespace pql {

// Trusted, sequential execution authority. Portfolio must outlive the broker.
// Strategies receive a PortfolioSnapshot, never this portfolio reference.
class FakeBroker final : public Broker {
   public:
    // Fixed single-currency fee per full fill; negative configuration throws.
    FakeBroker(Portfolio& portfolio, Money fee);
    [[nodiscard]] Execution execute(const Order& order, const MarketState& market) override;

   private:
    Portfolio& portfolio_;
    Money fee_;
};

}  // namespace pql
