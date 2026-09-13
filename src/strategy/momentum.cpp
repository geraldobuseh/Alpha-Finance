#include "strategy/momentum.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

#include "strategy/buy_sizing.hpp"

namespace pql {

Momentum::Momentum(const std::vector<Symbol>& universe, TradingCosts costs)
    : universe_(universe), costs_(costs) {
    if (universe_.size() < 2) throw std::invalid_argument("Momentum requires at least two assets");
    std::sort(universe_.begin(), universe_.end(),
              [](const Symbol& a, const Symbol& b) { return a.value() < b.value(); });
    if (std::adjacent_find(universe_.begin(), universe_.end()) != universe_.end()) {
        throw std::invalid_argument("Duplicate momentum universe symbol");
    }
}

std::vector<MomentumRank> Momentum::rank(const MarketState& market) const {
    std::vector<MomentumRank> ranking;
    std::vector<Date> sessions;
    for (const auto& symbol : universe_) {
        const auto* asset = market.asset(symbol);
        if (!asset || asset->history.size() < 21) throw MomentumError("Missing 21-session history");
        const auto first = asset->history.size() - 21;
        if (sessions.empty()) {
            for (std::size_t i = first; i < asset->history.size(); ++i) {
                sessions.push_back(asset->history[i].date());
            }
        } else {
            for (std::size_t i = 0; i < 21; ++i) {
                if (asset->history[first + i].date() != sessions[i]) {
                    throw MomentumError("Momentum session windows are not aligned");
                }
            }
        }
        const double initial = asset->history[first].close().value();
        const double final = asset->history.back().close().value();
        const double ratio = final / initial;
        const double change = ratio - 1.0;
        if (!std::isfinite(ratio) || ratio <= 0.0 || !std::isfinite(change) || change <= -1.0 ||
            (initial != final && change == 0.0)) {
            throw MomentumError("Unrepresentable momentum return");
        }
        ranking.push_back({symbol, change});
    }
    std::sort(ranking.begin(), ranking.end(), [](const MomentumRank& a, const MomentumRank& b) {
        if (a.return20 != b.return20) return a.return20 > b.return20;
        return a.symbol.value() < b.symbol.value();
    });
    return ranking;
}

std::vector<Order> Momentum::generateOrders(const MarketState& market,
                                            const PortfolioState& portfolio) {
    const auto day = std::chrono::floor<std::chrono::days>(market.timestamp().value());
    if (std::chrono::weekday{day} != std::chrono::Monday) return {};
    const auto& history = portfolio.transactionHistory();
    if (!history.empty() && history.back().timestamp() > market.timestamp()) {
        throw MomentumError("Portfolio history is after decision time");
    }
    std::int64_t highest_id = 0;
    for (const auto& event : history) {
        // Valid only for dedicated accounts using all-or-nothing batch execution.
        if (std::chrono::floor<std::chrono::days>(event.timestamp().value()) == day) return {};
        highest_id = std::max(highest_id, event.order_id().value());
    }
    const auto ranking = rank(market);
    std::vector<const Position*> holdings;
    for (const auto& position : portfolio.positions()) {
        if (position.quantity().value() > 0.0) holdings.push_back(&position);
    }
    std::sort(holdings.begin(), holdings.end(), [](const Position* a, const Position* b) {
        return a->symbol().value() < b->symbol().value();
    });
    const auto remaining_ids = std::numeric_limits<std::int64_t>::max() - highest_id;
    if (remaining_ids < 2 || holdings.size() > static_cast<std::uint64_t>(remaining_ids - 2)) {
        throw MomentumError("Order ID range exhausted");
    }
    std::vector<Order> orders;
    double cash = portfolio.cashBalance().value();
    const double fee = costs_.commission().value();
    for (const auto* position : holdings) {
        const auto* asset = market.asset(position->symbol());
        if (!asset) throw MomentumError("Missing held-asset quote");
        const auto fill = costs_.executionPrice(asset->price, OrderSide::Sell);
        if (!fill) throw MomentumError("Invalid sell price");
        const double notional = position->quantity().value() * fill->value();
        const double net = notional - fee;
        const double next_cash = cash + net;
        if (!std::isfinite(notional) || notional <= 0.0 || !std::isfinite(next_cash) ||
            next_cash < 0.0 || (fee > 0.0 && net == notional) || net == -fee ||
            (net != 0.0 && next_cash == cash) || (net > 0.0 && cash > 0.0 && next_cash <= net)) {
            throw MomentumError("Unrepresentable sale proceeds");
        }
        cash = next_cash;
        orders.push_back(Order::create_market(OrderId::create(++highest_id).value(),
                                              position->symbol(), OrderSide::Sell,
                                              position->quantity(), market.timestamp())
                             .value());
    }
    // Equal TOTAL cash budgets; each includes its own buy commission.
    const double first_budget = cash / 2.0;
    const double budgets[2]{first_budget, cash - first_budget};
    for (std::size_t i = 0; i < 2; ++i) {
        const auto* asset = market.asset(ranking[i].symbol);
        const auto fill = costs_.executionPrice(asset->price, OrderSide::Buy);
        if (!fill) throw MomentumError("Invalid buy price");
        const auto size = detail::sizeCashBudgetBuy(Money::create(budgets[i]).value(), *fill,
                                                    costs_.commission());
        if (!size) throw MomentumError("Insufficient or unrepresentable target budget");
        orders.push_back(Order::create_market(OrderId::create(++highest_id).value(), asset->symbol,
                                              OrderSide::Buy, *size, market.timestamp())
                             .value());
    }
    return orders;
}

}  // namespace pql
