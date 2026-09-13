#include "strategy/momentum.hpp"

// Interface must be self-contained before test dependencies.
#include <gtest/gtest.h>

#include <algorithm>
#include <limits>

#include "simulation/paper_portfolio_pair.hpp"
#include "strategy/dollar_cost_averaging.hpp"

namespace {
using namespace pql;
Money money(double value) { return Money::create(value).value(); }
Price price(double value) { return Price::create(value).value(); }
Symbol symbol(const char* text) { return Symbol::create(text).value(); }
OrderId oid(std::int64_t value) { return OrderId::create(value).value(); }
Timestamp time(int offset = 0) {
    return Timestamp{std::chrono::sys_days{std::chrono::year{2026} / 9 / 14} +
                     std::chrono::days{offset}};
}
Date date(std::chrono::sys_days day) {
    const std::chrono::year_month_day ymd{day};
    return Date::create(int(ymd.year()), unsigned(ymd.month()), unsigned(ymd.day())).value();
}
PriceBar bar(const Symbol& ticker, Date day, double close) {
    return PriceBar::create(ticker, day, price(close), price(close), price(close), price(close),
                            Quantity::create(1).value())
        .value();
}
MarketAsset series(const char* ticker, double first, double last, int offset = 0,
                   double current = 99, std::size_t count = 21) {
    std::vector<Date> days;
    auto day = std::chrono::floor<std::chrono::days>(time(offset).value());
    while (days.size() < count) {
        day -= std::chrono::days{1};
        const std::chrono::weekday weekday{day};
        if (weekday != std::chrono::Saturday && weekday != std::chrono::Sunday)
            days.push_back(date(day));
    }
    std::reverse(days.begin(), days.end());
    MarketAsset asset{symbol(ticker), price(current), {}};
    for (std::size_t i = 0; i < count; ++i) {
        asset.history.push_back(
            bar(asset.symbol, days[i], first + (last - first) * double(i) / double(count - 1)));
    }
    return asset;
}
std::vector<Symbol> universe() { return {symbol("NVDA"), symbol("MSFT"), symbol("AAPL")}; }
std::vector<MarketAsset> assets(int day = 0, bool rotate = false) {
    return {series("AAPL", rotate ? 100 : 50, rotate ? 90 : 100, day),
            series("MSFT", 75, 100, day),
            series("NVDA", rotate ? 25 : 100, 100, day),
            {symbol("SPY"), price(100), {}}};
}
MarketState snapshot(int day = 0, bool rotate = false) {
    return MarketState{time(day), assets(day, rotate)};
}
TradingCosts costs(double fee = 0, double bps = 0) { return TradingCosts{money(fee), bps}; }
Portfolio funded() {
    return Portfolio::create(PortfolioId::create(1).value(), money(1000)).value();
}
PaperPortfolioPair pair(TradingCosts friction = costs()) {
    return PaperPortfolioPair::create(PortfolioId::create(1).value(),
                                      PortfolioId::create(2).value(), oid(1), snapshot(), friction)
        .value();
}

TEST(Momentum, SyntheticReturnsClearlyRankWinnersAndUseTwentyIntervals) {
    Momentum strategy{universe(), costs()};
    const auto market = snapshot();
    const auto ranking = strategy.rank(market);
    ASSERT_EQ(ranking.size(), 3U);
    EXPECT_EQ(ranking[0].symbol, symbol("AAPL"));
    EXPECT_DOUBLE_EQ(ranking[0].return20, 1.0);
    EXPECT_EQ(ranking[1].symbol, symbol("MSFT"));
    EXPECT_NEAR(ranking[1].return20, 1.0 / 3.0, 1e-15);
    EXPECT_EQ(ranking[2].symbol, symbol("NVDA"));
    EXPECT_DOUBLE_EQ(ranking[2].return20, 0);
    auto extended = assets();
    for (auto& asset : extended) {
        if (asset.history.empty()) continue;
        const auto earlier =
            std::chrono::sys_days{asset.history.front().date().value()} - std::chrono::days{1};
        asset.history.insert(asset.history.begin(), bar(asset.symbol, date(earlier), 1));
    }
    const auto longer = strategy.rank(MarketState{time(), extended});
    for (std::size_t i = 0; i < ranking.size(); ++i) {
        EXPECT_EQ(longer[i].symbol, ranking[i].symbol);
        EXPECT_DOUBLE_EQ(longer[i].return20, ranking[i].return20);
    }
}

TEST(Momentum, ProposesTopTwoWithoutMutationAndAtomicExecutionRecordsCosts) {
    auto account = pair(costs(5));
    Momentum strategy{universe(), costs(5)};
    Strategy& interface = strategy;
    EXPECT_EQ(interface.name(), "20-Day Top-2 Momentum");
    const auto before = account.paperState();
    const auto control = account.benchmarkState();
    const auto orders = interface.generateOrders(snapshot(), before);
    ASSERT_EQ(orders.size(), 2U);
    EXPECT_EQ(orders[0].symbol(), symbol("AAPL"));
    EXPECT_EQ(orders[1].symbol(), symbol("MSFT"));
    for (const auto& order : orders) {
        EXPECT_EQ(order.side(), OrderSide::Buy);
        EXPECT_DOUBLE_EQ(order.quantity().value(), 5);
    }
    EXPECT_EQ(account.paperState(), before);
    const auto fills = account.executePaperBatch(orders, snapshot());
    ASSERT_TRUE(fills);
    ASSERT_EQ(fills->size(), 2U);
    for (const auto& fill : *fills) {
        EXPECT_TRUE(fill.isFilled());
        EXPECT_EQ(fill.fill()->price, price(99));
        EXPECT_EQ(fill.fill()->fees, money(5));
    }
    const auto after = account.paperState();
    EXPECT_EQ(after.cashBalance(), money(0));
    EXPECT_EQ(after.totalValue({{symbol("AAPL"), price(99)}, {symbol("MSFT"), price(99)}}),
              money(990));
    EXPECT_EQ(account.benchmarkState(), control);
    const auto replay =
        Portfolio::replay(after.id(), after.startingCash(), after.transactionHistory());
    ASSERT_TRUE(replay);
    EXPECT_EQ(replay->snapshot(), after);
}

TEST(Momentum, WeeklyRotationSellsBeforeBuyingAndChargesTurnover) {
    auto account = pair(costs(5));
    Momentum strategy{universe(), costs(5)};
    ASSERT_TRUE(account.executePaperBatch(strategy.generateOrders(snapshot(), account.paperState()),
                                          snapshot()));
    EXPECT_TRUE(strategy.generateOrders(snapshot(), account.paperState()).empty());
    EXPECT_TRUE(strategy.generateOrders(snapshot(1), account.paperState()).empty());
    Momentum recreated{universe(), costs(5)};
    EXPECT_TRUE(recreated.generateOrders(snapshot(), account.paperState()).empty());
    const auto monday = snapshot(7, true);
    const auto orders = recreated.generateOrders(monday, account.paperState());
    ASSERT_EQ(orders.size(), 4U);
    EXPECT_EQ(orders[0].side(), OrderSide::Sell);
    EXPECT_EQ(orders[0].symbol(), symbol("AAPL"));
    EXPECT_EQ(orders[1].side(), OrderSide::Sell);
    EXPECT_EQ(orders[1].symbol(), symbol("MSFT"));
    EXPECT_EQ(orders[2].symbol(), symbol("NVDA"));
    EXPECT_EQ(orders[3].symbol(), symbol("MSFT"));
    ASSERT_TRUE(account.executePaperBatch(orders, monday));
    const auto after = account.paperState();
    EXPECT_EQ(after.transactionHistory().size(), 6U);
    EXPECT_EQ(after.totalValue({{symbol("AAPL"), price(99)},
                                {symbol("MSFT"), price(99)},
                                {symbol("NVDA"), price(99)}}),
              money(970));
    for (const auto& position : after.positions()) {
        if (position.symbol() == symbol("AAPL")) {
            EXPECT_DOUBLE_EQ(position.quantity().value(), 0);
        }
    }
    EXPECT_TRUE(recreated.generateOrders(monday, after).empty());
}

TEST(Momentum, LaterLegRejectionRollsBackSalesAndAllowsSameMondayRetry) {
    auto account = pair();
    Momentum strategy{universe(), costs()};
    ASSERT_TRUE(account.executePaperBatch(strategy.generateOrders(snapshot(), account.paperState()),
                                          snapshot()));
    const auto before = account.paperState();
    const auto control = account.benchmarkState();
    const auto monday = snapshot(7, true);
    const auto orders = strategy.generateOrders(monday, before);
    ASSERT_EQ(orders.size(), 4U);
    auto rejected = orders;
    rejected.back() =
        Order::create_market(rejected.back().id(), rejected.back().symbol(), OrderSide::Buy,
                             Quantity::create(10000).value(), monday.timestamp())
            .value();
    EXPECT_FALSE(account.executePaperBatch(rejected, monday));
    EXPECT_EQ(account.paperState(), before);
    EXPECT_EQ(account.benchmarkState(), control);
    Momentum restarted{universe(), costs()};
    EXPECT_EQ(restarted.generateOrders(monday, account.paperState()), orders);
    ASSERT_TRUE(account.executePaperBatch(orders, monday));
    const auto committed = account.paperState();
    EXPECT_FALSE(account.executePaperBatch(orders, monday));
    EXPECT_EQ(account.paperState(), committed);
}

TEST(Momentum, TiesAndAllNegativeReturnsHaveDeterministicTopTwo) {
    Momentum strategy{universe(), costs()};
    std::vector<MarketAsset> data{series("NVDA", 100, 90), series("MSFT", 100, 90),
                                  series("AAPL", 100, 80)};
    const auto ranked = strategy.rank(MarketState{time(), data});
    EXPECT_EQ(ranked[0].symbol, symbol("MSFT"));
    EXPECT_EQ(ranked[1].symbol, symbol("NVDA"));
    EXPECT_LT(ranked[0].return20, 0);
    std::reverse(data.begin(), data.end());
    const auto orders = strategy.generateOrders(MarketState{time(), data}, funded().snapshot());
    ASSERT_EQ(orders.size(), 2U);
    EXPECT_EQ(orders[0].symbol(), ranked[0].symbol);
    EXPECT_EQ(orders[1].symbol(), ranked[1].symbol);
    EXPECT_THROW((Momentum{{symbol("AAPL")}, costs()}), std::invalid_argument);
    EXPECT_THROW((Momentum{{symbol("AAPL"), symbol("AAPL")}, costs()}), std::invalid_argument);
}

TEST(Momentum, MissingShortOrMisalignedHistoryNeverShrinksUniverseSilently) {
    Momentum strategy{universe(), costs()};
    auto short_data = assets();
    short_data[0].history.erase(short_data[0].history.begin());
    EXPECT_THROW((void)strategy.rank(MarketState{time(), short_data}), MomentumError);
    auto missing = assets();
    missing.erase(missing.begin());
    EXPECT_THROW((void)strategy.rank(MarketState{time(), missing}), MomentumError);
    auto different = assets();
    different[0] = series("AAPL", 50, 100, -7);
    EXPECT_THROW((void)strategy.rank(MarketState{time(), different}), MomentumError);
}

TEST(Momentum, SnapshotRejectsFutureCurrentDuplicateAndWrongSymbolBars) {
    auto data = assets();
    data.push_back(data.front());
    EXPECT_THROW((MarketState{time(), data}), std::invalid_argument);
    for (int offset : {0, 1}) {
        data = assets();
        data[0].history.push_back(
            bar(data[0].symbol, date(std::chrono::floor<std::chrono::days>(time(offset).value())),
                101));
        EXPECT_THROW((MarketState{time(), data}), std::invalid_argument);
    }
    data = assets();
    data[0].history.push_back(data[0].history.back());
    EXPECT_THROW((MarketState{time(), data}), std::invalid_argument);
    data = assets();
    data[0].history[0] = bar(symbol("WRONG"), data[0].history[0].date(), 100);
    EXPECT_THROW((MarketState{time(), data}), std::invalid_argument);
    data = assets();
    std::swap(data[0].history[0], data[0].history[1]);
    EXPECT_THROW((MarketState{time(), data}), std::invalid_argument);
    EXPECT_THROW((MarketState{time(), {}}), std::invalid_argument);
}

TEST(Momentum, SnapshotOwnsHistoryAndSingleAssetStrategiesFindTheirQuote) {
    auto data = assets();
    const MarketState owned{time(), data};
    data[0].history.clear();
    EXPECT_EQ(owned.asset(symbol("AAPL"))->history.size(), 21U);
    EXPECT_THROW((void)owned.symbol(), std::logic_error);
    EXPECT_THROW((void)owned.price(), std::logic_error);
    EXPECT_EQ(owned.asset(symbol("SPY"))->price, price(100));
    DollarCostAveraging dca{costs()};
    const auto orders = dca.generateOrders(owned, funded().snapshot());
    ASSERT_EQ(orders.size(), 1U);
    EXPECT_EQ(orders[0].symbol(), symbol("SPY"));
}

TEST(Momentum, MissingHeldQuoteFuturePortfolioAndIdOverflowRejectWholePlan) {
    auto portfolio = funded();
    FakeBroker broker{portfolio, costs()};
    const MarketState earlier{symbol("OTHER"), price(10), time(-7)};
    const auto request = Order::create_market(oid(50), symbol("OTHER"), OrderSide::Buy,
                                              Quantity::create(1).value(), time(-7))
                             .value();
    ASSERT_TRUE(broker.execute(request, earlier).isFilled());
    Momentum strategy{universe(), costs()};
    EXPECT_THROW((void)strategy.generateOrders(snapshot(), portfolio.snapshot()), MomentumError);
    auto with_held = assets();
    with_held.push_back({symbol("OTHER"), price(10), {}});
    const auto plan = strategy.generateOrders(MarketState{time(), with_held}, portfolio.snapshot());
    ASSERT_EQ(plan.size(), 3U);
    EXPECT_EQ(plan[0].symbol(), symbol("OTHER"));
    EXPECT_EQ(plan[0].id(), oid(51));
    EXPECT_THROW((void)strategy.generateOrders(snapshot(-14), portfolio.snapshot()), MomentumError);
    auto exhausted = funded();
    FakeBroker other{exhausted, costs()};
    const auto last =
        Order::create_market(oid(std::numeric_limits<std::int64_t>::max() - 1), symbol("OTHER"),
                             OrderSide::Buy, Quantity::create(1).value(), time(-7))
            .value();
    ASSERT_TRUE(other.execute(last, earlier).isFilled());
    EXPECT_THROW(
        (void)strategy.generateOrders(MarketState{time(), with_held}, exhausted.snapshot()),
        MomentumError);
}

TEST(Momentum, ArithmeticAndInfeasibleBudgetsRejectBeforeAnyProposalEscapes) {
    Momentum strategy{universe(), costs()};
    for (bool overflow : {true, false}) {
        auto data = assets();
        data[0].history.front() =
            bar(data[0].symbol, data[0].history.front().date(),
                overflow ? std::numeric_limits<double>::min() : std::numeric_limits<double>::max());
        data[0].history.back() =
            bar(data[0].symbol, data[0].history.back().date(),
                overflow ? std::numeric_limits<double>::max() : std::numeric_limits<double>::min());
        EXPECT_THROW((void)strategy.rank(MarketState{time(), data}), MomentumError);
    }
    Momentum expensive{universe(), costs(500)};
    EXPECT_THROW((void)expensive.generateOrders(snapshot(), funded().snapshot()), MomentumError);
    auto account = pair(costs(1, 100));
    Momentum friction{universe(), costs(1, 100)};
    const auto proposals = friction.generateOrders(snapshot(), account.paperState());
    const auto result = account.executePaperBatch(proposals, snapshot());
    ASSERT_TRUE(result);
    for (const auto& fill : *result) {
        EXPECT_DOUBLE_EQ(fill.fill()->price.value(), 99.99);
        EXPECT_EQ(fill.fill()->fees, money(1));
    }
    EXPECT_GE(account.paperState().cashBalance().value(), 0);
}

}  // namespace
