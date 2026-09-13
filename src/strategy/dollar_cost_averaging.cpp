#include "strategy/dollar_cost_averaging.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>

#include "strategy/buy_sizing.hpp"

namespace pql {

std::vector<Order> DollarCostAveraging::generateOrders(const MarketState& market,
                                                       const PortfolioState& portfolio) {
    const auto day = std::chrono::floor<std::chrono::days>(market.timestamp().value());
    const auto* spy = market.asset(Symbol::create("SPY").value());
    if (!spy || std::chrono::weekday{day} != std::chrono::Monday) return {};
    const auto& history = portfolio.transactionHistory();
    if (!history.empty() && history.back().timestamp() > market.timestamp()) {
        throw std::invalid_argument("DCA portfolio history is later than the decision time");
    }
    std::int64_t highest_id = 0;
    for (const auto& event : history) {
        if (event.symbol() == spy->symbol && event.side() == OrderSide::Buy &&
            std::chrono::floor<std::chrono::days>(event.timestamp().value()) == day)
            return {};
        highest_id = std::max(highest_id, event.order_id().value());
    }
    if (highest_id == std::numeric_limits<std::int64_t>::max() ||
        portfolio.cashBalance().value() < 25.0)
        return {};
    const auto fill_price = costs_.executionPrice(spy->price, OrderSide::Buy);
    if (!fill_price) return {};
    const auto quantity =
        detail::sizeCashBudgetBuy(Money::create(25.0).value(), *fill_price, costs_.commission());
    if (!quantity) return {};
    const auto next_id = OrderId::create(highest_id + 1).value();
    return {
        Order::create_market(next_id, spy->symbol, OrderSide::Buy, *quantity, market.timestamp())
            .value()};
}

}  // namespace pql
