#pragma once

#include "domain/portfolio_state.hpp"
#include "execution/fake_broker.hpp"

namespace pql {

// Canonical in-memory paper setup: equal $1,000 funding and an invested SPY control.
// No externally visible pair exists unless its initial benchmark fill succeeds.
class PaperPortfolioPair {
   public:
    [[nodiscard]] static std::optional<PaperPortfolioPair> create(PortfolioId paper_id,
                                                                  PortfolioId benchmark_id,
                                                                  OrderId benchmark_order_id,
                                                                  const MarketState& initial_spy,
                                                                  TradingCosts costs);
    PaperPortfolioPair(const PaperPortfolioPair&) = default;
    PaperPortfolioPair& operator=(const PaperPortfolioPair&) = delete;

    [[nodiscard]] PortfolioState paperState() const { return paper_.snapshot(); }
    [[nodiscard]] PortfolioState benchmarkState() const { return benchmark_.snapshot(); }
    [[nodiscard]] Timestamp startTime() const noexcept { return start_; }
    [[nodiscard]] Execution executePaper(const Order& order, const MarketState& market);
    // All-or-nothing synchronous batch. nullopt rejects with no financial changes;
    // successful receipts are published only after every order has filled.
    [[nodiscard]] std::optional<std::vector<Execution>> executePaperBatch(
        const std::vector<Order>& orders, const MarketState& market);

   private:
    PaperPortfolioPair(const Portfolio& paper, const Portfolio& benchmark, Timestamp start,
                       TradingCosts costs)
        : paper_(paper), benchmark_(benchmark), start_(start), costs_(costs) {}
    Portfolio paper_;
    Portfolio benchmark_;
    Timestamp start_;
    TradingCosts costs_;
};

}  // namespace pql
