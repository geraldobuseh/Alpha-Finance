#pragma once

#include <stdexcept>

#include "domain/market_data.hpp"
#include "domain/portfolio.hpp"

namespace pql {

class ValuationError : public std::runtime_error {
   public:
    using std::runtime_error::runtime_error;
};

// Completed raw USD-equity session valuation. Caller supplies the exchange close
// timestamp (on the UTC session date) and consecutive intended trading sessions.
// No deposits/withdrawals or corporate-action accounting exist in the ledger yet.
class DailyValuation {
   public:
    [[nodiscard]] static DailyValuation calculate(const PortfolioSnapshot& state, Date session,
                                                  Timestamp close,
                                                  const std::vector<PriceBar>& bars,
                                                  const DailyValuation* previous = nullptr);

    [[nodiscard]] PortfolioId portfolioId() const { return portfolio_id_; }
    [[nodiscard]] Date session() const { return session_; }
    [[nodiscard]] Timestamp asOf() const { return as_of_; }
    [[nodiscard]] Money startingCash() const { return starting_cash_; }
    [[nodiscard]] Money cash() const { return cash_; }
    [[nodiscard]] Money positionValue() const { return position_value_; }
    [[nodiscard]] Money totalValue() const { return total_value_; }
    // Fractional simple returns. Absence means no baseline or a zero denominator.
    [[nodiscard]] std::optional<double> dailyReturn() const { return daily_return_; }
    [[nodiscard]] std::optional<double> cumulativeReturn() const { return cumulative_return_; }
    [[nodiscard]] std::optional<Timestamp> previousAsOf() const { return previous_as_of_; }
    [[nodiscard]] std::size_t ledgerSequence() const { return ledger_sequence_; }
    [[nodiscard]] const std::vector<MarketPrice>& marks() const { return marks_; }

   private:
    DailyValuation(PortfolioId id, Date session, Timestamp close, Money starting, Money cash,
                   Money positions, Money total)
        : portfolio_id_(id),
          session_(session),
          as_of_(close),
          starting_cash_(starting),
          cash_(cash),
          position_value_(positions),
          total_value_(total) {}
    PortfolioId portfolio_id_;
    Date session_;
    Timestamp as_of_;
    Money starting_cash_;
    Money cash_;
    Money position_value_;
    Money total_value_;
    std::optional<double> daily_return_;
    std::optional<double> cumulative_return_;
    std::optional<Timestamp> previous_as_of_;
    std::size_t ledger_sequence_{};
    std::vector<MarketPrice> marks_;
};

}  // namespace pql
