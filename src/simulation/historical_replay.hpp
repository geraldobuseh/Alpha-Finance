#pragma once

#include <stdexcept>

#include "analytics/daily_valuation.hpp"

namespace pql {

class ReplayError : public std::runtime_error {
   public:
    using std::runtime_error::runtime_error;
};

struct HistoricalPortfolio {
    PortfolioSnapshot state;
    DailyValuation valuation;
};

// Pure reconstruction from an authoritative ledger, not strategy re-execution.
// Inputs are owned by callers; returned observations own their values.
class HistoricalReplay {
   public:
    // Inclusive cutoff. Validate the complete supplied ledger (including future
    // suffix), then replay its prefix in original order. Never sort tied events.
    [[nodiscard]] static PortfolioSnapshot at(PortfolioId id, Money starting_cash,
                                              const std::vector<Transaction>& transactions,
                                              Timestamp cutoff);

    // Caller supplies a completed exchange close on the UTC session date.
    // Select exact-session raw closes from a multi-session history; never use a
    // stale or future price as fallback. Missing/duplicate marks raise ValuationError.
    // First daily return is absent; cumulative return uses original starting cash.
    [[nodiscard]] static HistoricalPortfolio atClose(PortfolioId id, Money starting_cash,
                                                     const std::vector<Transaction>& transactions,
                                                     const std::vector<PriceBar>& historical_prices,
                                                     Date session, Timestamp close);
};

}  // namespace pql
