#include <gtest/gtest.h>

#include <cmath>
#include <limits>

#include "execution/fake_broker.hpp"
#include "strategy/mean_reversion.hpp"

namespace {
using namespace pql;
Money money(double x) { return Money::create(x).value(); }
Price price(double x) { return Price::create(x).value(); }
Symbol symbol(const char* x = "AAPL") { return Symbol::create(x).value(); }
Timestamp time(int offset = 0) {
    return Timestamp{std::chrono::sys_days{std::chrono::year{2026} / 9 / 14} +
                     std::chrono::days{offset}};
}
MarketState market(double quote, std::vector<double> closes = std::vector<double>(20, 100),
                   int offset = 0) {
    MarketAsset asset{symbol(), price(quote), {}};
    auto day = std::chrono::floor<std::chrono::days>(time(offset).value()) -
               std::chrono::days{static_cast<int>(closes.size())};
    for (double close : closes) {
        const std::chrono::year_month_day ymd{day};
        const auto date =
            Date::create(int(ymd.year()), unsigned(ymd.month()), unsigned(ymd.day())).value();
        asset.history.push_back(PriceBar::create(symbol(), date, price(close), price(close),
                                                 price(close), price(close),
                                                 Quantity::create(1).value())
                                    .value());
        day += std::chrono::days{1};
    }
    return MarketState{time(offset), {asset}};
}
TradingCosts costs(double fee = 0, double bps = 0) { return TradingCosts{money(fee), bps}; }
Portfolio funded(double cash = 1000) {
    return Portfolio::create(PortfolioId::create(1).value(), money(cash)).value();
}
void buy(Portfolio& portfolio, std::int64_t id = 1, int offset = -1, const char* ticker = "AAPL") {
    FakeBroker broker{portfolio, costs()};
    const MarketState quote{symbol(ticker), price(90), time(offset)};
    const auto order =
        Order::create_market(OrderId::create(id).value(), symbol(ticker), OrderSide::Buy,
                             Quantity::create(2).value(), time(offset))
            .value();
    ASSERT_TRUE(broker.execute(order, quote).isFilled());
}

TEST(MeanReversion, StrictEntryAndNoShorting) {
    MeanReversion strategy{symbol(), costs()};
    Strategy& interface = strategy;
    EXPECT_EQ(interface.name(), "20-Day Mean Reversion");
    auto account = funded();
    const auto before = account.snapshot();
    const auto snapshot = market(std::nextafter(95.0, 0.0));
    const auto orders = interface.generateOrders(snapshot, before);
    ASSERT_EQ(orders.size(), 1U);
    EXPECT_EQ(orders[0].side(), OrderSide::Buy);
    EXPECT_EQ(orders[0].symbol(), symbol());
    EXPECT_EQ(orders[0].timestamp(), time());
    EXPECT_EQ(interface.generateOrders(snapshot, before), orders);
    EXPECT_EQ(account.snapshot(), before);
    for (double quote : {95.0, 99.0, 100.0, 101.0})
        EXPECT_TRUE(interface.generateOrders(market(quote), before).empty());
}

TEST(MeanReversion, InclusiveExitAndNoPyramiding) {
    auto account = funded();
    buy(account);
    MeanReversion strategy{symbol(), costs()};
    for (double quote : {90.0, 95.0, 99.0, std::nextafter(100.0, 0.0)})
        EXPECT_TRUE(strategy.generateOrders(market(quote), account.snapshot()).empty());
    for (double quote : {100.0, 101.0}) {
        const auto orders = strategy.generateOrders(market(quote), account.snapshot());
        ASSERT_EQ(orders.size(), 1U);
        EXPECT_EQ(orders[0].side(), OrderSide::Sell);
        EXPECT_DOUBLE_EQ(orders[0].quantity().value(), 2);
        EXPECT_EQ(orders[0].id(), OrderId::create(2).value());
    }
}

TEST(MeanReversion, UsesOnlyLatestTwentyClosesAndExcludesQuote) {
    MeanReversion strategy{symbol(), costs()};
    const auto account = funded().snapshot();
    std::vector<double> closes(10, 80);
    closes.insert(closes.end(), 10, 120);  // Arithmetic MA = 100.
    EXPECT_TRUE(strategy.generateOrders(market(95, closes), account).empty());
    EXPECT_EQ(strategy.generateOrders(market(94, closes), account).size(), 1U);
    closes.insert(closes.begin(), 10000);
    EXPECT_TRUE(strategy.generateOrders(market(95, closes), account).empty());
    EXPECT_EQ(strategy.generateOrders(market(94, closes), account).size(), 1U);
    EXPECT_THROW((void)strategy.generateOrders(market(94, std::vector<double>(19, 100)), account),
                 MeanReversionError);
    EXPECT_THROW(
        (void)strategy.generateOrders(MarketState{symbol("SPY"), price(94), time()}, account),
        MeanReversionError);
}

TEST(MeanReversion, CostsExecutionReplayAndReentry) {
    const auto friction = costs(1, 100);
    MeanReversion strategy{symbol(), friction};
    auto account = funded();
    FakeBroker broker{account, friction};
    const auto entry = market(90);
    const auto orders = strategy.generateOrders(entry, account.snapshot());
    ASSERT_EQ(orders.size(), 1U);
    const auto fill = broker.execute(orders[0], entry);
    ASSERT_TRUE(fill.isFilled());
    EXPECT_DOUBLE_EQ(fill.fill()->price.value(), 90.9);
    EXPECT_EQ(fill.fill()->fees, money(1));
    EXPECT_GE(account.cashBalance().value(), 0);
    EXPECT_TRUE(strategy.generateOrders(entry, account.snapshot()).empty());
    MeanReversion restarted{symbol(), friction};
    const auto exit = market(100, std::vector<double>(20, 100), 1);
    const auto exits = restarted.generateOrders(exit, account.snapshot());
    ASSERT_EQ(exits.size(), 1U);
    ASSERT_TRUE(broker.execute(exits[0], exit).isFilled());
    EXPECT_NEAR(account.cashBalance().value(), orders[0].quantity().value() * 99 - 1, 1e-10);
    const auto replay =
        Portfolio::replay(account.id(), account.startingCash(), account.transactionHistory());
    ASSERT_TRUE(replay);
    EXPECT_EQ(replay->snapshot(), account.snapshot());
    EXPECT_TRUE(restarted.generateOrders(exit, replay->snapshot()).empty());
    const auto next =
        restarted.generateOrders(market(94, std::vector<double>(20, 100), 2), replay->snapshot());
    ASSERT_EQ(next.size(), 1U);
    EXPECT_EQ(next[0].id(), OrderId::create(3).value());
}

TEST(MeanReversion, RejectedExecutionCanRetryWithoutPrivateState) {
    auto account = funded();
    MeanReversion strategy{symbol(), costs()};
    const auto before = account.snapshot();
    const auto orders = strategy.generateOrders(market(90), before);
    ASSERT_EQ(orders.size(), 1U);
    FakeBroker broker{account, costs()};
    EXPECT_FALSE(broker.execute(orders[0], market(200)).isFilled());
    EXPECT_EQ(account.snapshot(), before);
    EXPECT_EQ(strategy.generateOrders(market(90), before), orders);
}

TEST(MeanReversion, CashMissingDataFutureStateAndExhaustedIds) {
    MeanReversion strategy{symbol(), costs(1)};
    for (double cash : {0.0, 0.5, 1.0})
        EXPECT_TRUE(strategy.generateOrders(market(90), funded(cash).snapshot()).empty());
    auto future = funded();
    buy(future, 1, 1);
    EXPECT_THROW((void)strategy.generateOrders(market(90), future.snapshot()), MeanReversionError);
    auto exhausted = funded();
    buy(exhausted, std::numeric_limits<std::int64_t>::max(), -1, "OTHER");
    EXPECT_THROW((void)strategy.generateOrders(market(90), exhausted.snapshot()),
                 MeanReversionError);
    auto other = funded();
    buy(other, 40, -1, "OTHER");
    const auto orders = strategy.generateOrders(market(90), other.snapshot());
    ASSERT_EQ(orders.size(), 1U);
    EXPECT_EQ(orders[0].side(), OrderSide::Buy);
    EXPECT_EQ(orders[0].id(), OrderId::create(41).value());
    EXPECT_TRUE(strategy.generateOrders(market(100), other.snapshot()).empty());
}

TEST(MeanReversion, LargeFiniteClosesAvoidSumOverflowAndTinyThresholdFailsExplicitly) {
    MeanReversion strategy{symbol(), costs()};
    const auto huge = std::numeric_limits<double>::max();
    EXPECT_TRUE(
        strategy.generateOrders(market(huge, std::vector<double>(20, huge)), funded().snapshot())
            .empty());
    const auto tiny = std::numeric_limits<double>::denorm_min();
    EXPECT_THROW((void)strategy.generateOrders(market(tiny, std::vector<double>(20, tiny)),
                                               funded().snapshot()),
                 MeanReversionError);
}
}  // namespace
