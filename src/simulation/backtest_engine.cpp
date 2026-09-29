#include "simulation/backtest_engine.hpp"

#include <algorithm>
#include <optional>
#include <utility>

#include "execution/fake_broker.hpp"

namespace pql {
namespace {

void validateDay(const HistoricalTradingDay& day) {
    if (day.market.timestamp() != day.close) {
        throw BacktestError("Market snapshot must be taken at the trading-day close");
    }
    if (std::chrono::floor<std::chrono::days>(day.close.value()) !=
        std::chrono::sys_days{day.session.value()}) {
        throw BacktestError("Trading-day close must belong to its session");
    }
    if (day.closing_prices.size() != day.market.assets().size()) {
        throw BacktestError("Every market quote requires exactly one closing price");
    }
    for (auto bar_it = day.closing_prices.begin(); bar_it != day.closing_prices.end(); ++bar_it) {
        const auto& bar = *bar_it;
        if (bar.date() != day.session) {
            throw BacktestError("Closing price has wrong trading session");
        }
        const auto* quote = day.market.asset(bar.symbol());
        if (!quote || quote->price != bar.close()) {
            throw BacktestError("Closing price must match its market quote at the same close");
        }
        if (std::any_of(day.closing_prices.begin(), bar_it, [&](const PriceBar& previous) {
                return previous.symbol() == bar.symbol();
            })) {
            throw BacktestError("Duplicate closing price symbol");
        }
    }
}

std::vector<Execution> executeDay(const std::vector<Order>& orders, const MarketState& market,
                                  const TradingCosts& costs, Portfolio& portfolio) {
    auto candidate = portfolio;
    FakeBroker broker{candidate, costs};
    std::vector<Execution> receipts;
    receipts.reserve(orders.size());
    for (const auto& order : orders) {
        const auto execution = broker.execute(order, market);
        if (!execution.isFilled()) {
            throw BacktestError(
                "Backtest order " + std::to_string(order.id().value()) + " rejected at timestamp " +
                std::to_string(market.timestamp().value().time_since_epoch().count()) +
                " (ExecutionRejection=" +
                std::to_string(static_cast<int>(*execution.rejectionReason())) + ")");
        }
        receipts.push_back(execution);
    }
    portfolio = candidate;
    return receipts;
}

void validateTradingDays(const std::vector<HistoricalTradingDay>& days) {
    if (days.empty()) throw BacktestError("Backtest requires at least one trading day");
    std::optional<Date> previous_session;
    std::optional<Timestamp> previous_close;
    for (const auto& day : days) {
        validateDay(day);
        if ((previous_session && day.session <= *previous_session) ||
            (previous_close && day.close <= *previous_close)) {
            throw BacktestError("Trading days must be strictly increasing");
        }
        previous_session = day.session;
        previous_close = day.close;
    }
}

}  // namespace

HistoricalMarketData::HistoricalMarketData(std::vector<HistoricalTradingDay> days)
    : days_(std::move(days)) {}

BacktestEngine::BacktestEngine(TradingCosts costs) : costs_(costs) {}

BacktestResult BacktestEngine::run(Strategy& strategy, HistoricalMarketData& data,
                                   Portfolio initialPortfolio) const {
    Portfolio portfolio = initialPortfolio;
    const auto initial_snapshot = portfolio.snapshot();
    validateTradingDays(data.tradingDays());
    if (!portfolio.transactionHistory().empty() &&
        portfolio.transactionHistory().back().timestamp() > data.tradingDays().front().close) {
        throw BacktestError("Initial portfolio contains transactions after the first decision");
    }
    std::vector<BacktestDailySnapshot> snapshots;
    snapshots.reserve(data.tradingDays().size());
    std::optional<DailyValuation> previous;

    for (const auto& day : data.tradingDays()) {
        const auto orders = strategy.generateOrders(day.market, portfolio.snapshot());
        auto executions = executeDay(orders, day.market, costs_, portfolio);
        DailyValuation valuation =
            DailyValuation::calculate(portfolio.snapshot(), day.session, day.close,
                                      day.closing_prices, previous ? &*previous : nullptr);
        previous = valuation;
        snapshots.push_back(BacktestDailySnapshot{day.session, day.close, portfolio.snapshot(),
                                                  std::move(executions), std::move(valuation)});
    }

    return BacktestResult{strategy.name(), initial_snapshot, portfolio.snapshot(),
                          std::move(snapshots)};
}

BacktestResult run(Strategy& strategy, HistoricalMarketData& data, Portfolio initialPortfolio) {
    return BacktestEngine{}.run(strategy, data, initialPortfolio);
}

}  // namespace pql
