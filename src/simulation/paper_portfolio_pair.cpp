#include "simulation/paper_portfolio_pair.hpp"

#include "strategy/spy_buy_and_hold.hpp"

namespace pql {

std::optional<PaperPortfolioPair> PaperPortfolioPair::create(PortfolioId paper_id,
                                                             PortfolioId benchmark_id,
                                                             OrderId benchmark_order_id,
                                                             const MarketState& initial_spy,
                                                             TradingCosts costs) {
    if (paper_id == benchmark_id || initial_spy.symbol().value() != "SPY") return std::nullopt;
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

}  // namespace pql
