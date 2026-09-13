#include "strategy/spy_buy_and_hold.hpp"

#include "strategy/buy_sizing.hpp"

namespace pql {

std::vector<Order> SpyBuyAndHold::generateOrders(const MarketState& market,
                                                 const PortfolioState& portfolio) {
    const auto* spy = market.asset(Symbol::create("SPY").value());
    if (!spy || !portfolio.transactionHistory().empty() || !portfolio.positions().empty())
        return {};
    const auto fill_price = costs_.executionPrice(spy->price, OrderSide::Buy);
    if (!fill_price) return {};
    const auto quantity =
        detail::sizeCashBudgetBuy(portfolio.cashBalance(), *fill_price, costs_.commission());
    if (!quantity) return {};
    const auto order = Order::create_market(initial_order_, spy->symbol, OrderSide::Buy, *quantity,
                                            market.timestamp());
    return {*order};
}

}  // namespace pql
