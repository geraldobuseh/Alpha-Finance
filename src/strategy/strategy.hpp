#pragma once

#include <string>
#include <vector>

#include "domain/order.hpp"
#include "domain/portfolio_state.hpp"
#include "execution/market_state.hpp"

namespace pql {

// Observe, decide, propose. No broker or mutable portfolio is supplied here.
// Callers own inputs; implementations must not retain borrowed input references.
class Strategy {
   public:
    // Owned proposals, not fills or reservations. Empty means no action.
    // Orchestration submits orders to a Broker for validation and execution;
    // accepted transactions then record portfolio consequences.
    [[nodiscard]] virtual std::vector<Order> generateOrders(const MarketState& market,
                                                            const PortfolioState& portfolio) = 0;
    [[nodiscard]] virtual std::string name() const = 0;
    virtual ~Strategy() = default;
};

}  // namespace pql
