#include "analytics/daily_valuation.hpp"

#include <algorithm>
#include <cmath>

namespace pql {
namespace {
std::optional<double> simpleReturn(Money current, Money base) {
    if (base.value() == 0.0) return std::nullopt;
    // Subtraction before division retains small changes better than ratio - 1.
    const double result = (current.value() - base.value()) / base.value();
    if (!std::isfinite(result) || result < -1.0 || (current != base && result == 0.0) ||
        (current.value() > 0.0 && result == -1.0)) {
        throw ValuationError("Unrepresentable portfolio return");
    }
    return result;
}
}  // namespace

DailyValuation DailyValuation::calculate(const PortfolioSnapshot& state, Date session,
                                         Timestamp close, const std::vector<PriceBar>& bars,
                                         const DailyValuation* previous) {
    if (std::chrono::floor<std::chrono::days>(close.value()) !=
        std::chrono::sys_days{session.value()}) {
        throw ValuationError("Close timestamp must be on the supplied UTC session date");
    }
    const auto& history = state.transactionHistory();
    if (!history.empty() && history.back().timestamp() > close) {
        throw ValuationError("Portfolio contains transactions after valuation close");
    }
    if (previous &&
        (previous->portfolioId() != state.id() ||
         previous->startingCash() != state.startingCash() || previous->session() >= session ||
         previous->asOf() >= close || previous->ledgerSequence() > history.size())) {
        throw ValuationError("Inconsistent previous valuation");
    }
    if (previous) {
        std::vector<Transaction> prefix;
        for (const auto& event : history) {
            if (event.timestamp() > previous->asOf()) break;
            prefix.push_back(event);
        }
        const auto baseline = Portfolio::replay(state.id(), state.startingCash(), prefix);
        if (!baseline || prefix.size() != previous->ledgerSequence() ||
            baseline->cashBalance() != previous->cash() ||
            baseline->marketValue(previous->marks()) != previous->positionValue()) {
            throw ValuationError("Previous valuation does not match the ledger prefix");
        }
    }
    std::vector<MarketPrice> marks;
    for (const auto& bar : bars) {
        if (bar.date() != session) throw ValuationError("Closing price has wrong session");
        marks.push_back({bar.symbol(), bar.close()});
    }
    const auto positions = state.marketValue(marks);
    const auto total = state.totalValue(marks);
    if (!positions || !total) throw ValuationError("Missing, duplicate or unrepresentable marks");
    DailyValuation result{state.id(),          session,    close, state.startingCash(),
                          state.cashBalance(), *positions, *total};
    result.ledger_sequence_ = history.size();
    result.cumulative_return_ = simpleReturn(*total, state.startingCash());
    if (previous) {
        result.previous_as_of_ = previous->asOf();
        result.daily_return_ = simpleReturn(*total, previous->totalValue());
    }
    // Retain only prices actually used; input ordering does not affect provenance.
    for (const auto& position : state.positions()) {
        if (position.quantity().value() == 0.0) continue;
        const auto mark = std::find_if(marks.begin(), marks.end(), [&](const MarketPrice& item) {
            return item.symbol == position.symbol();
        });
        result.marks_.push_back(*mark);
    }
    std::sort(result.marks_.begin(), result.marks_.end(),
              [](const auto& a, const auto& b) { return a.symbol.value() < b.symbol.value(); });
    return result;
}
}  // namespace pql
