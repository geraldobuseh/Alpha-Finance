#include "execution/fake_broker.hpp"

#include <algorithm>

namespace pql {

FakeBroker::FakeBroker(Portfolio& portfolio, Money fee)
    : FakeBroker(portfolio, TradingCosts{fee, 0.0}) {}

FakeBroker::FakeBroker(Portfolio& portfolio, TradingCosts costs)
    : portfolio_(portfolio), costs_(costs) {}

Execution FakeBroker::execute(const Order& order, const MarketState& market) {
    const auto reject = [&](ExecutionRejection reason) noexcept {
        return Execution::rejected(portfolio_.id(), order.id(), reason);
    };
    // Order/Symbol factories already exclude zero quantities and malformed symbols.
    if (order.symbol() != market.symbol()) return reject(ExecutionRejection::UnknownSymbol);
    const auto& history = portfolio_.transactionHistory();
    if (market.timestamp() < order.timestamp() ||
        (!history.empty() && market.timestamp() < history.back().timestamp())) {
        return reject(ExecutionRejection::InvalidTimestamp);
    }
    if (std::any_of(history.begin(), history.end(), [&](const Transaction& previous) {
            return previous.order_id() == order.id();
        })) {
        return reject(ExecutionRejection::DuplicateOrder);
    }
    if (order.side() == OrderSide::Sell) {
        const auto& positions = portfolio_.positions();
        const auto owned = std::find_if(
            positions.begin(), positions.end(),
            [&](const Position& position) { return position.symbol() == order.symbol(); });
        if (owned == positions.end() || owned->quantity().value() < order.quantity().value()) {
            return reject(ExecutionRejection::InsufficientShares);
        }
    }
    auto execution_price = std::optional<Price>{market.price()};
    if (costs_.slippageBasisPoints() > 0.0) {
        const double quote = market.price().value();
        const double delta = quote * (costs_.slippageBasisPoints() / 10000.0);
        const double adjusted = order.side() == OrderSide::Buy ? quote + delta : quote - delta;
        execution_price = Price::create(adjusted);
        // Never silently turn a positive configured cost into zero friction.
        if (delta <= 0.0 || !execution_price ||
            (order.side() == OrderSide::Buy ? adjusted <= quote : adjusted >= quote)) {
            return reject(ExecutionRejection::InvalidArithmetic);
        }
    }
    const auto commission = costs_.commission();
    const auto notional = Money::create(order.quantity().value() * execution_price->value());
    if (!notional || notional->value() <= 0.0) return reject(ExecutionRejection::InvalidArithmetic);
    const double amount = order.side() == OrderSide::Buy ? notional->value() + commission.value()
                                                         : commission.value() - notional->value();
    if (!Money::create(amount)) return reject(ExecutionRejection::InvalidArithmetic);
    if (amount > portfolio_.cashBalance().value())
        return reject(ExecutionRejection::InsufficientCash);

    const auto trade = Trade::create(order, *execution_price, market.timestamp());
    const auto transaction = Transaction::create(portfolio_.id(), *trade, commission);
    // All allocations (including the ledger's staged update) happen before commit.
    // Returning this scalar receipt cannot throw, even when NRVO is disabled.
    const auto result = Execution::accepted(*transaction);
    if (!portfolio_.applyTransaction(*transaction)) {
        return reject(ExecutionRejection::PortfolioRejected);
    }
    return result;
}

}  // namespace pql
