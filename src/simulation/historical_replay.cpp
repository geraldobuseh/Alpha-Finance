#include "simulation/historical_replay.hpp"

namespace pql {

PortfolioSnapshot HistoricalReplay::at(PortfolioId id, Money starting_cash,
                                       const std::vector<Transaction>& transactions,
                                       Timestamp cutoff) {
    // Reject corrupt suffixes as invalid input rather than hiding them behind a
    // cutoff. The resulting full state is never used to mark a historical date.
    if (!Portfolio::replay(id, starting_cash, transactions)) {
        throw ReplayError("Invalid starting cash or transaction history");
    }
    std::vector<Transaction> prefix;
    for (const auto& event : transactions) {
        if (event.timestamp() > cutoff) break;
        prefix.push_back(event);
    }
    const auto portfolio = Portfolio::replay(id, starting_cash, prefix);
    if (!portfolio) throw ReplayError("Invalid historical ledger prefix");
    return portfolio->snapshot();
}

HistoricalPortfolio HistoricalReplay::atClose(PortfolioId id, Money starting_cash,
                                              const std::vector<Transaction>& transactions,
                                              const std::vector<PriceBar>& historical_prices,
                                              Date session, Timestamp close) {
    const auto state = at(id, starting_cash, transactions, close);
    std::vector<PriceBar> closing_bars;
    for (const auto& bar : historical_prices) {
        if (bar.date() == session) closing_bars.push_back(bar);
    }
    return {state, DailyValuation::calculate(state, session, close, closing_bars)};
}

}  // namespace pql
