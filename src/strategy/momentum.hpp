#pragma once

#include <stdexcept>

#include "execution/trading_costs.hpp"
#include "strategy/strategy.hpp"

namespace pql {

class MomentumError : public std::runtime_error {
   public:
    using std::runtime_error::runtime_error;
};

struct MomentumRank {
    Symbol symbol;
    double return20;  // Fractional simple return, e.g. 0.10 means 10%.
};

// Dedicated sequential account: Monday full liquidation and equal-budget top-two
// reentry. Proposals MUST use atomic batch execution with these same quotes/costs.
class Momentum final : public Strategy {
   public:
    Momentum(const std::vector<Symbol>& universe, TradingCosts costs);
    Momentum(const Momentum&) = default;
    Momentum& operator=(const Momentum&) = delete;
    [[nodiscard]] std::string name() const override { return "20-Day Top-2 Momentum"; }
    [[nodiscard]] std::vector<MomentumRank> rank(const MarketState& market) const;
    [[nodiscard]] std::vector<Order> generateOrders(const MarketState& market,
                                                    const PortfolioState& portfolio) override;

   private:
    std::vector<Symbol> universe_;
    TradingCosts costs_;
};

}  // namespace pql
