#include <gtest/gtest.h>

#include <algorithm>
#include <limits>

#include "simulation/historical_replay.hpp"

namespace {
using namespace pql;
Money money(double value) { return Money::create(value).value(); }
Price price(double value) { return Price::create(value).value(); }
PortfolioId pid(std::int64_t id = 1) { return PortfolioId::create(id).value(); }
Symbol symbol(const char* text = "AAPL") { return Symbol::create(text).value(); }
Date day(unsigned d) { return Date::create(2026, 9, d).value(); }
Timestamp time(unsigned d, int hour = 20) {
    return Timestamp{std::chrono::sys_days{day(d).value()} + std::chrono::hours{hour}};
}
Transaction event(std::int64_t id, OrderSide side, double units, double fill, unsigned d,
                  double fee = 0, const char* ticker = "AAPL", std::int64_t owner = 1) {
    const auto order = Order::create_market(OrderId::create(id).value(), symbol(ticker), side,
                                            Quantity::create(units).value(), time(d))
                           .value();
    return Transaction::create(pid(owner), Trade::create(order, price(fill), time(d)).value(),
                               money(fee))
        .value();
}
PriceBar bar(unsigned d, double close, const char* ticker = "AAPL") {
    return PriceBar::create(symbol(ticker), day(d), price(close), price(close), price(close),
                            price(close), Quantity::create(0).value())
        .value();
}
std::vector<Transaction> ledger() {
    return {event(50, OrderSide::Buy, 2, 100, 14, 1), event(4, OrderSide::Sell, 1, 120, 15, 1),
            event(90, OrderSide::Sell, 1, 80, 16, 1)};
}

TEST(HistoricalReplay, ReconstructsCashHoldingsCostBasisAndRealizedPnlAtEachCutoff) {
    const auto events = ledger();
    const auto before = HistoricalReplay::at(pid(), money(1000), events, time(14, 19));
    EXPECT_EQ(before.cashBalance(), money(1000));
    EXPECT_TRUE(before.positions().empty());
    EXPECT_TRUE(before.transactionHistory().empty());
    const auto bought = HistoricalReplay::atClose(pid(), money(1000), events,
                                                  {bar(14, 110), bar(15, 9000)}, day(14), time(14));
    EXPECT_EQ(bought.state.cashBalance(), money(799));
    ASSERT_EQ(bought.state.positions().size(), 1U);
    EXPECT_DOUBLE_EQ(bought.state.positions()[0].quantity().value(), 2);
    EXPECT_EQ(bought.state.positions()[0].average_cost(), price(100.5));
    EXPECT_EQ(bought.state.positions()[0].cost_basis(), money(201));
    EXPECT_EQ(bought.state.positions()[0].unrealized_pnl(price(110)), money(19));
    EXPECT_EQ(bought.valuation.positionValue(), money(220));
    EXPECT_EQ(bought.valuation.totalValue(), money(1019));
    EXPECT_EQ(bought.valuation.ledgerSequence(), 1U);
    EXPECT_FALSE(bought.valuation.dailyReturn());
    EXPECT_DOUBLE_EQ(*bought.valuation.cumulativeReturn(), 0.019);
    const auto sold =
        HistoricalReplay::atClose(pid(), money(1000), events, {bar(15, 120)}, day(15), time(15));
    EXPECT_EQ(sold.state.cashBalance(), money(918));
    EXPECT_DOUBLE_EQ(sold.state.positions()[0].quantity().value(), 1);
    EXPECT_EQ(sold.state.positions()[0].average_cost(), price(100.5));
    EXPECT_EQ(sold.state.positions()[0].cost_basis(), money(100.5));
    EXPECT_EQ(sold.state.positions()[0].realized_pnl(), money(18.5));
    EXPECT_EQ(sold.valuation.totalValue(), money(1038));
    const auto flat = HistoricalReplay::atClose(pid(), money(1000), events, {}, day(16), time(16));
    EXPECT_EQ(flat.state.cashBalance(), money(997));
    EXPECT_EQ(flat.valuation.positionValue(), money(0));
    EXPECT_EQ(flat.state.positions()[0].realized_pnl(), money(-3));
    EXPECT_FALSE(flat.state.positions()[0].average_cost());
    EXPECT_EQ(flat.state.positions()[0].cost_basis(), money(0));
}

TEST(HistoricalReplay, MillisecondCutoffIsInclusiveAndMatchesDirectPrefixReplay) {
    const auto events = ledger();
    const auto tick = events[1].timestamp();
    const auto before = HistoricalReplay::at(
        pid(), money(1000), events, Timestamp{tick.value() - std::chrono::milliseconds{1}});
    const auto exact = HistoricalReplay::at(pid(), money(1000), events, tick);
    const auto after = HistoricalReplay::at(
        pid(), money(1000), events, Timestamp{tick.value() + std::chrono::milliseconds{1}});
    const auto first_prefix = Portfolio::replay(pid(), money(1000), {events[0]});
    const auto second_prefix = Portfolio::replay(pid(), money(1000), {events[0], events[1]});
    ASSERT_TRUE(first_prefix);
    ASSERT_TRUE(second_prefix);
    EXPECT_EQ(before, first_prefix->snapshot());
    EXPECT_EQ(exact, second_prefix->snapshot());
    EXPECT_EQ(after, exact);
    EXPECT_EQ(before.transactionHistory().size(), 1U);
    EXPECT_EQ(exact.transactionHistory().size(), 2U);
}

TEST(HistoricalReplay, IntradayStateCutoffDoesNotIncludeLaterSameDayFills) {
    const std::vector<Transaction> events{event(1, OrderSide::Buy, 2, 100, 14),
                                          event(2, OrderSide::Sell, 1, 120, 15)};
    const auto noon = HistoricalReplay::at(pid(), money(1000), events, time(15, 12));
    EXPECT_EQ(noon.cashBalance(), money(800));
    ASSERT_EQ(noon.positions().size(), 1U);
    EXPECT_DOUBLE_EQ(noon.positions()[0].quantity().value(), 2);
    EXPECT_EQ(noon.transactionHistory().size(), 1U);
}

TEST(HistoricalReplay, RepeatedRunsExactlyMatchCompleteOwnedStateAndValuation) {
    auto events = ledger();
    auto prices = std::vector<PriceBar>{bar(13, 1), bar(14, 110), bar(15, 10000)};
    const auto original_events = events;
    const auto original_prices = prices;
    const auto a = HistoricalReplay::atClose(pid(), money(1000), events, prices, day(14), time(14));
    const auto b = HistoricalReplay::atClose(pid(), money(1000), events, prices, day(14), time(14));
    EXPECT_EQ(a.state, b.state);
    EXPECT_EQ(a.valuation.cash(), b.valuation.cash());
    EXPECT_EQ(a.valuation.positionValue(), b.valuation.positionValue());
    EXPECT_EQ(a.valuation.totalValue(), b.valuation.totalValue());
    EXPECT_EQ(a.valuation.dailyReturn(), b.valuation.dailyReturn());
    EXPECT_EQ(a.valuation.cumulativeReturn(), b.valuation.cumulativeReturn());
    EXPECT_EQ(a.valuation.asOf(), b.valuation.asOf());
    EXPECT_EQ(a.valuation.session(), b.valuation.session());
    EXPECT_EQ(a.valuation.previousAsOf(), b.valuation.previousAsOf());
    EXPECT_EQ(a.valuation.ledgerSequence(), b.valuation.ledgerSequence());
    ASSERT_EQ(a.valuation.marks().size(), b.valuation.marks().size());
    for (std::size_t i = 0; i < a.valuation.marks().size(); ++i) {
        EXPECT_EQ(a.valuation.marks()[i].symbol, b.valuation.marks()[i].symbol);
        EXPECT_EQ(a.valuation.marks()[i].price, b.valuation.marks()[i].price);
    }
    EXPECT_EQ(events, original_events);
    EXPECT_EQ(prices, original_prices);
    events.clear();
    prices.clear();
    EXPECT_EQ(a.state.transactionHistory().size(), 1U);
    EXPECT_EQ(a.valuation.marks()[0].price, price(110));
}

TEST(HistoricalReplay, ValidFutureSuffixCannotChangeHistoricalResult) {
    const std::vector<Transaction> prefix{ledger().front()};
    const auto short_run =
        HistoricalReplay::atClose(pid(), money(1000), prefix, {bar(14, 110)}, day(14), time(14));
    const auto long_run = HistoricalReplay::atClose(
        pid(), money(1000), ledger(), {bar(16, 9000), bar(13, 1), bar(14, 110)}, day(14), time(14));
    EXPECT_EQ(short_run.state, long_run.state);
    EXPECT_EQ(short_run.valuation.totalValue(), long_run.valuation.totalValue());
    EXPECT_EQ(short_run.valuation.cumulativeReturn(), long_run.valuation.cumulativeReturn());
}

TEST(HistoricalReplay, SameTimestampEventsKeepLedgerOrderNotOrderIdOrder) {
    const std::vector<Transaction> events{event(50, OrderSide::Buy, 1, 100, 14),
                                          event(1, OrderSide::Sell, 1, 110, 14)};
    const auto result = HistoricalReplay::at(pid(), money(100), events, time(14));
    EXPECT_EQ(result.cashBalance(), money(110));
    EXPECT_EQ(result.transactionHistory(), events);
    const std::vector<Transaction> wrong{events[1], events[0]};
    EXPECT_THROW((void)HistoricalReplay::at(pid(), money(100), wrong, time(14)), ReplayError);
}

TEST(HistoricalReplay, MissingStaleFutureAndDuplicateClosingPricesReject) {
    for (const auto& prices : std::vector<std::vector<PriceBar>>{
             {}, {bar(13, 110)}, {bar(15, 110)}, {bar(14, 110), bar(14, 111)}}) {
        EXPECT_THROW((void)HistoricalReplay::atClose(pid(), money(1000), ledger(), prices, day(14),
                                                     time(14)),
                     ValuationError);
    }
    EXPECT_THROW((void)HistoricalReplay::atClose(pid(), money(1000), ledger(), {bar(14, 110)},
                                                 day(14), time(15)),
                 ValuationError);
}

TEST(HistoricalReplay, EntireLedgerMustBeValidEvenBeyondCutoff) {
    const auto buy = event(1, OrderSide::Buy, 1, 100, 14);
    const std::vector<std::vector<Transaction>> invalid{
        {buy, buy},
        {buy, event(2, OrderSide::Sell, 2, 100, 15)},
        {buy, event(2, OrderSide::Buy, 1, 100, 15, 0, "AAPL", 2)},
        {event(2, OrderSide::Buy, 1, 100, 15), buy}};
    for (const auto& events : invalid)
        EXPECT_THROW((void)HistoricalReplay::at(pid(), money(1000), events, time(13)), ReplayError);
    EXPECT_THROW((void)HistoricalReplay::at(pid(), money(-1), {}, time(14)), ReplayError);
}

TEST(HistoricalReplay, EmptyAndZeroCapitalAndUnrepresentableMarks) {
    const auto empty = HistoricalReplay::atClose(pid(), money(0), {}, {}, day(14), time(14));
    EXPECT_EQ(empty.valuation.totalValue(), money(0));
    EXPECT_FALSE(empty.valuation.cumulativeReturn());
    EXPECT_THROW((void)HistoricalReplay::atClose(pid(), money(1000), ledger(),
                                                 {bar(14, std::numeric_limits<double>::max())},
                                                 day(14), time(14)),
                 ValuationError);
}
}  // namespace
