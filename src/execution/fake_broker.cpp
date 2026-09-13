#include "execution/fake_broker.hpp"

#include <algorithm>
#include <stdexcept>

namespace pql {

FakeBroker::FakeBroker(Portfolio& portfolio, Money fee) : portfolio_(portfolio), fee_(fee) {
    if (fee.value() < 0.0) throw std::invalid_argument("Broker fee must be nonnegative");
}

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
    const auto notional = Money::create(order.quantity().value() * market.price().value());
    if (!notional || notional->value() <= 0.0) return reject(ExecutionRejection::InvalidArithmetic);
    const double amount = order.side() == OrderSide::Buy ? notional->value() + fee_.value()
                                                         : fee_.value() - notional->value();
    if (!Money::create(amount)) return reject(ExecutionRejection::InvalidArithmetic);
    if (amount > portfolio_.cashBalance().value())
        return reject(ExecutionRejection::InsufficientCash);

    const auto trade = Trade::create(order, market.price(), market.timestamp());
    const auto transaction = Transaction::create(portfolio_.id(), *trade, fee_);
    // All allocations (including the ledger's staged update) happen before commit.
    // Returning this scalar receipt cannot throw, even when NRVO is disabled.
    const auto result = Execution::accepted(*transaction);
    if (!portfolio_.applyTransaction(*transaction)) {
        return reject(ExecutionRejection::PortfolioRejected);
    }
    return result;
}

}  // namespace pql
