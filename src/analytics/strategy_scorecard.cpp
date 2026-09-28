#include "analytics/strategy_scorecard.hpp"

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

#include "analytics/returns.hpp"

namespace pql {
namespace {
double checkedAdd(double total, double value, const char* message) {
    const double result = total + value;
    if (!std::isfinite(result) || (value != 0.0 && result == total) ||
        (total != 0.0 && result == value)) {
        throw ScorecardError(message);
    }
    return result;
}

double multiply(double left, double right, const char* message) {
    const double result = left * right;
    if (!std::isfinite(result) || (left != 0.0 && right != 0.0 && result == 0.0)) {
        throw ScorecardError(message);
    }
    return result;
}

void validateCurves(const std::vector<EquityPoint>& strategy, const std::vector<EquityPoint>& spy) {
    if (strategy.empty() || strategy.size() != spy.size()) {
        throw ScorecardError("Strategy and SPY equity curves must be nonempty and aligned");
    }
    for (std::size_t index = 0; index < strategy.size(); ++index) {
        if (strategy[index].value.value() < 0.0 || spy[index].value.value() < 0.0) {
            throw ScorecardError("Scorecard equity values must be nonnegative");
        }
        if (strategy[index].timestamp != spy[index].timestamp) {
            throw ScorecardError("Strategy and SPY observation timestamps must match");
        }
        if (index > 0 && strategy[index - 1].timestamp >= strategy[index].timestamp) {
            throw ScorecardError("Scorecard observations must be strictly increasing");
        }
    }
}

std::vector<double> periodReturns(const std::vector<EquityPoint>& curve) {
    std::vector<double> result;
    result.reserve(curve.size() - 1);
    for (std::size_t index = 1; index < curve.size(); ++index) {
        const auto value = dailyReturn(curve[index].value, curve[index - 1].value);
        if (!value) {
            throw ScorecardError("An internal scorecard return has a zero denominator");
        }
        result.push_back(*value);
    }
    return result;
}

double meanEquity(const std::vector<EquityPoint>& curve) {
    const double scale =
        std::max_element(curve.begin(), curve.end(), [](const auto& left, const auto& right) {
            return left.value.value() < right.value.value();
        })->value.value();
    if (scale == 0.0) throw ScorecardError("Turnover requires positive mean equity");
    double scaled_total = 0.0;
    for (const auto& point : curve) {
        scaled_total =
            checkedAdd(scaled_total, point.value.value() / scale, "Unrepresentable mean equity");
    }
    return scale * (scaled_total / static_cast<double>(curve.size()));
}

bool hasOpenPosition(const PortfolioSnapshot& snapshot) {
    return std::any_of(snapshot.positions().begin(), snapshot.positions().end(),
                       [](const Position& position) { return position.quantity().value() > 0.0; });
}

void validateLedgerWindow(const PortfolioSnapshot& terminal_portfolio,
                          const std::vector<EquityPoint>& strategy_equity) {
    Timestamp previous = strategy_equity.front().timestamp;
    for (const auto& transaction : terminal_portfolio.transactionHistory()) {
        if (transaction.portfolio_id() != terminal_portfolio.id() ||
            transaction.timestamp() <= strategy_equity.front().timestamp ||
            transaction.timestamp() > strategy_equity.back().timestamp ||
            transaction.timestamp() < previous) {
            throw ScorecardError("Terminal ledger does not match the scorecard window");
        }
        previous = transaction.timestamp();
    }
}

void validateEquityAgainstLedger(const PortfolioSnapshot& terminal_portfolio,
                                 const std::vector<EquityPoint>& strategy_equity) {
    std::vector<Transaction> prefix;
    const auto& transactions = terminal_portfolio.transactionHistory();
    auto next = transactions.begin();

    for (const auto& point : strategy_equity) {
        while (next != transactions.end() && next->timestamp() <= point.timestamp) {
            prefix.push_back(*next);
            ++next;
        }

        const auto replay =
            Portfolio::replay(terminal_portfolio.id(), terminal_portfolio.startingCash(), prefix);
        if (!replay) {
            throw ScorecardError("Terminal ledger cannot be replayed through scorecard window");
        }

        const auto snapshot = replay->snapshot();
        if (point.value.value() < snapshot.cashBalance().value()) {
            throw ScorecardError("Strategy equity cannot be below replayed cash balance");
        }
        if (!hasOpenPosition(snapshot) && point.value != snapshot.cashBalance()) {
            throw ScorecardError("Cash-only strategy equity must equal replayed cash balance");
        }
    }
}
}  // namespace

StrategyScorecard buildStrategyScorecard(std::string strategy_name,
                                         const PortfolioSnapshot& terminal_portfolio,
                                         const std::vector<EquityPoint>& strategy_equity,
                                         const std::vector<EquityPoint>& spy_equity,
                                         std::optional<std::uint32_t> periods_per_year) {
    try {
        if (strategy_name.empty()) throw ScorecardError("Strategy name must not be empty");
        validateCurves(strategy_equity, spy_equity);
        if (strategy_equity.front().value != terminal_portfolio.startingCash()) {
            throw ScorecardError("Strategy curve must start at the portfolio's starting capital");
        }
        validateLedgerWindow(terminal_portfolio, strategy_equity);
        validateEquityAgainstLedger(terminal_portfolio, strategy_equity);

        const auto total =
            cumulativeReturn(strategy_equity.back().value, strategy_equity.front().value);
        const auto benchmark = cumulativeReturn(spy_equity.back().value, spy_equity.front().value);
        if (!total || !benchmark)
            throw ScorecardError("Scorecard starting capital must be positive");
        const auto drawdown = maximumDrawdown(strategy_equity);
        if (!drawdown) throw ScorecardError("Scorecard drawdown requires positive observed equity");

        double fees = 0.0;
        double gross_notional = 0.0;
        const auto& transactions = terminal_portfolio.transactionHistory();
        for (const auto& transaction : transactions) {
            fees = checkedAdd(fees, transaction.fees().value(), "Unrepresentable total fees");
            const double notional =
                multiply(transaction.quantity().value(), transaction.price().value(),
                         "Unrepresentable trade notional");
            gross_notional =
                checkedAdd(gross_notional, notional, "Unrepresentable gross turnover notional");
        }

        const double average_equity = meanEquity(strategy_equity);
        const double turnover = gross_notional / average_equity;
        if (!std::isfinite(turnover) || (gross_notional != 0.0 && turnover == 0.0)) {
            throw ScorecardError("Unrepresentable turnover");
        }
        const auto fee_money = Money::create(fees);
        if (!fee_money) throw ScorecardError("Unrepresentable total fees");

        const auto returns = periodReturns(strategy_equity);
        return StrategyScorecard{std::move(strategy_name),
                                 strategy_equity.front().value,
                                 strategy_equity.back().value,
                                 *total,
                                 *benchmark,
                                 pql::excess_return(*total, *benchmark),
                                 *drawdown,
                                 sampleVolatility(returns, periods_per_year),
                                 sampleSharpe(returns, periods_per_year),
                                 transactions.size(),
                                 turnover,
                                 *fee_money};
    } catch (const ScorecardError&) {
        throw;
    } catch (const std::runtime_error& error) {
        throw ScorecardError(error.what());
    }
}

}  // namespace pql
