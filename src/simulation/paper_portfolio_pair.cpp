#include "simulation/paper_portfolio_pair.hpp"

#include <type_traits>
#include <utility>

#include "strategy/spy_buy_and_hold.hpp"

namespace pql {

std::optional<PaperPortfolioPair> PaperPortfolioPair::create(PortfolioId paper_id,
                                                             PortfolioId benchmark_id,
                                                             OrderId benchmark_order_id,
                                                             const MarketState& initial_spy,
                                                             TradingCosts costs) {
    if (paper_id == benchmark_id || !initial_spy.asset(Symbol::create("SPY").value()))
        return std::nullopt;
    const auto starting_cash = Money::create(1000.0).value();
    const auto paper = Portfolio::create(paper_id, starting_cash).value();
    auto benchmark = Portfolio::create(benchmark_id, starting_cash).value();
    SpyBuyAndHold strategy{benchmark_order_id, costs};
    const auto proposals = strategy.generateOrders(initial_spy, benchmark.snapshot());
    if (proposals.size() != 1) return std::nullopt;
    FakeBroker broker{benchmark, costs};
    if (!broker.execute(proposals.front(), initial_spy).isFilled()) return std::nullopt;
    return PaperPortfolioPair{paper, benchmark, initial_spy.timestamp(), costs};
}

Execution PaperPortfolioPair::executePaper(const Order& order, const MarketState& market) {
    if (order.timestamp() < start_ || market.timestamp() < start_) {
        return Execution::rejected(paper_.id(), order.id(), ExecutionRejection::InvalidTimestamp);
    }
    FakeBroker broker{paper_, costs_};
    return broker.execute(order, market);
}

std::optional<std::vector<Execution>> PaperPortfolioPair::executePaperBatch(
    const std::vector<Order>& orders, const MarketState& market) {
    if (market.timestamp() < start_) return std::nullopt;
    auto candidate = paper_;
    FakeBroker broker{candidate, costs_};
    std::vector<Execution> receipts;
    receipts.reserve(orders.size());
    for (const auto& order : orders) {
        if (order.timestamp() < start_) return std::nullopt;
        const auto execution = broker.execute(order, market);
        if (!execution.isFilled()) return std::nullopt;
        receipts.push_back(execution);
    }
    // Allocate all receipts before commit. Portfolio's snapshot assignment copies
    // before its noexcept swap; failure cannot partially replace financial state.
    auto result = std::optional<std::vector<Execution>>{std::move(receipts)};
    static_assert(std::is_nothrow_move_constructible_v<decltype(result)>);
    paper_ = candidate;
    return result;  // Non-const result moves without allocating after commit.
}

}  // namespace pql
