#include "strategy/mean_reversion.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

#include "strategy/buy_sizing.hpp"

namespace pql {

std::vector<Order> MeanReversion::generateOrders(const MarketState& market,
                                                 const PortfolioState& portfolio) {
    const auto& events = portfolio.transactionHistory();
    if (!events.empty() && events.back().timestamp() > market.timestamp()) {
        throw MeanReversionError("Portfolio history is after decision time");
    }
    const auto* asset = market.asset(symbol_);
    if (!asset || asset->history.size() < 20) {
        throw MeanReversionError("Missing 20-session history");
    }
    double scale = 0.0;
    for (std::size_t i = asset->history.size() - 20; i < asset->history.size(); ++i) {
        scale = std::max(scale, asset->history[i].close().value());
    }
    // Normalize before summing so twenty large finite closes cannot overflow.
    double sum = 0.0;
    for (std::size_t i = asset->history.size() - 20; i < asset->history.size(); ++i) {
        sum += asset->history[i].close().value() / scale;
    }
    const double average = scale * (sum / 20.0);
    const double entry = average * 0.95;
    if (!std::isfinite(average) || average <= 0.0 || entry <= 0.0 || entry >= average) {
        throw MeanReversionError("Unrepresentable moving average or threshold");
    }
    const auto held = std::find_if(portfolio.positions().begin(), portfolio.positions().end(),
                                   [&](const Position& p) { return p.symbol() == symbol_; });
    const bool is_long = held != portfolio.positions().end() && held->quantity().value() > 0.0;
    OrderSide side = OrderSide::Buy;
    std::optional<Quantity> quantity;
    if (is_long) {
        if (asset->price.value() < average) return {};
        side = OrderSide::Sell;
        quantity = held->quantity();
    } else {
        if (asset->price.value() >= entry) return {};
        if (portfolio.cashBalance().value() <= costs_.commission().value()) return {};
        const auto fill = costs_.executionPrice(asset->price, OrderSide::Buy);
        if (!fill) throw MeanReversionError("Invalid buy price");
        quantity = detail::sizeCashBudgetBuy(portfolio.cashBalance(), *fill, costs_.commission());
        if (!quantity) throw MeanReversionError("Unrepresentable buy budget");
    }
    std::int64_t highest_id = 0;
    for (const auto& event : events) highest_id = std::max(highest_id, event.order_id().value());
    if (highest_id == std::numeric_limits<std::int64_t>::max()) {
        throw MeanReversionError("Order ID range exhausted");
    }
    return {Order::create_market(OrderId::create(highest_id + 1).value(), symbol_, side, *quantity,
                                 market.timestamp())
                .value()};
}

}  // namespace pql
