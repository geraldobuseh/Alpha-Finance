#include <gtest/gtest.h>

#include <algorithm>
#include <limits>

#include "analytics/daily_valuation.hpp"

namespace {
using namespace pql;
Money money(double value) { return Money::create(value).value(); }
Price price(double value) { return Price::create(value).value(); }
Symbol symbol(const char* value = "AAPL") { return Symbol::create(value).value(); }
Date session(unsigned day = 14) { return Date::create(2026, 9, day).value(); }
Timestamp close(unsigned day = 14) {
    return Timestamp{std::chrono::sys_days{session(day).value()} + std::chrono::hours{20}};
}
Portfolio funded(double cash = 1000, std::int64_t id = 1) {
    return Portfolio::create(PortfolioId::create(id).value(), money(cash)).value();
}
PriceBar bar(double mark, unsigned day = 14, const char* ticker = "AAPL") {
    return PriceBar::create(symbol(ticker), session(day), price(mark), price(mark), price(mark),
                            price(mark), Quantity::create(1).value())
        .value();
}
Transaction event(std::int64_t id, OrderSide side, double units, double fill, double fee = 0,
                  unsigned day = 14, const char* ticker = "AAPL") {
    const auto order = Order::create_market(OrderId::create(id).value(), symbol(ticker), side,
                                            Quantity::create(units).value(), close(day))
                           .value();
    return Transaction::create(PortfolioId::create(1).value(),
                               Trade::create(order, price(fill), close(day)).value(), money(fee))
        .value();
}

TEST(DailyValuation, CashOnlyFirstObservationKeepsDailyReturnAbsent) {
    const auto state = funded().snapshot();
    const auto value = DailyValuation::calculate(state, session(), close(), {});
    EXPECT_EQ(value.portfolioId(), state.id());
    EXPECT_EQ(value.session(), session());
    EXPECT_EQ(value.asOf(), close());
    EXPECT_EQ(value.cash(), money(1000));
    EXPECT_EQ(value.positionValue(), money(0));
    EXPECT_EQ(value.totalValue(), money(1000));
    EXPECT_FALSE(value.dailyReturn());
    EXPECT_FALSE(value.previousAsOf());
    ASSERT_TRUE(value.cumulativeReturn());
    EXPECT_DOUBLE_EQ(*value.cumulativeReturn(), 0);
    EXPECT_EQ(value.ledgerSequence(), 0U);
    EXPECT_TRUE(value.marks().empty());
}

TEST(DailyValuation, FeesRemainInCashAndInitialCumulativeBaseline) {
    auto account = funded();
    ASSERT_TRUE(account.applyTransaction(event(1, OrderSide::Buy, 2, 100, 1)));
    const auto before = account.snapshot();
    const auto value = DailyValuation::calculate(before, session(), close(), {bar(100)});
    EXPECT_EQ(value.cash(), money(799));
    EXPECT_EQ(value.positionValue(), money(200));
    EXPECT_EQ(value.totalValue(), money(999));
    ASSERT_TRUE(value.cumulativeReturn());
    EXPECT_DOUBLE_EQ(*value.cumulativeReturn(), -0.001);
    EXPECT_FALSE(value.dailyReturn());
    EXPECT_EQ(value.ledgerSequence(), 1U);
    EXPECT_EQ(account.snapshot(), before);
    const auto gain =
        DailyValuation::calculate(before, session(15), close(15), {bar(110, 15)}, &value);
    EXPECT_EQ(gain.totalValue(), money(1019));
    ASSERT_TRUE(gain.dailyReturn());
    EXPECT_DOUBLE_EQ(*gain.dailyReturn(), 20.0 / 999.0);
    EXPECT_DOUBLE_EQ(*gain.cumulativeReturn(), 0.019);
}

TEST(DailyValuation, DailyReturnsCompoundRatherThanAdd) {
    auto account = funded();
    ASSERT_TRUE(account.applyTransaction(event(1, OrderSide::Buy, 10, 100)));
    const auto state = account.snapshot();
    const auto first = DailyValuation::calculate(state, session(), close(), {bar(100)});
    const auto second =
        DailyValuation::calculate(state, session(15), close(15), {bar(110, 15)}, &first);
    const auto third =
        DailyValuation::calculate(state, session(16), close(16), {bar(99, 16)}, &second);
    ASSERT_TRUE(second.dailyReturn());
    ASSERT_TRUE(third.dailyReturn());
    ASSERT_TRUE(third.cumulativeReturn());
    EXPECT_DOUBLE_EQ(*second.dailyReturn(), 0.1);
    EXPECT_DOUBLE_EQ(*third.dailyReturn(), -0.1);
    EXPECT_DOUBLE_EQ(*third.cumulativeReturn(), -0.01);
    EXPECT_EQ(third.previousAsOf(), second.asOf());
    EXPECT_NEAR((1 + *second.dailyReturn()) * (1 + *third.dailyReturn()) - 1,
                *third.cumulativeReturn(), 1e-15);
}

TEST(DailyValuation, ZeroFundingHasUndefinedReturnsAndTotalLossIsValid) {
    const auto empty = funded(0).snapshot();
    const auto initial = DailyValuation::calculate(empty, session(), close(), {});
    const auto later = DailyValuation::calculate(empty, session(15), close(15), {}, &initial);
    EXPECT_FALSE(initial.cumulativeReturn());
    EXPECT_FALSE(later.cumulativeReturn());
    EXPECT_FALSE(later.dailyReturn());
    auto account = funded(100);
    ASSERT_TRUE(account.applyTransaction(event(1, OrderSide::Buy, 1, 100)));
    const auto first =
        DailyValuation::calculate(account.snapshot(), session(), close(), {bar(100)});
    ASSERT_TRUE(account.applyTransaction(event(2, OrderSide::Sell, 1, 100, 100, 15)));
    const auto loss =
        DailyValuation::calculate(account.snapshot(), session(15), close(15), {}, &first);
    EXPECT_EQ(loss.totalValue(), money(0));
    ASSERT_TRUE(loss.dailyReturn());
    ASSERT_TRUE(loss.cumulativeReturn());
    EXPECT_DOUBLE_EQ(*loss.dailyReturn(), -1);
    EXPECT_DOUBLE_EQ(*loss.cumulativeReturn(), -1);
    EXPECT_TRUE(loss.marks().empty());
    const auto after =
        DailyValuation::calculate(account.snapshot(), session(16), close(16), {}, &loss);
    EXPECT_FALSE(after.dailyReturn());
    EXPECT_DOUBLE_EQ(*after.cumulativeReturn(), -1);
}

TEST(DailyValuation, RequiresCompleteUniqueCorrectSessionMarks) {
    auto account = funded();
    ASSERT_TRUE(account.applyTransaction(event(1, OrderSide::Buy, 2, 100)));
    const auto state = account.snapshot();
    EXPECT_THROW((void)DailyValuation::calculate(state, session(), close(), {}), ValuationError);
    EXPECT_THROW((void)DailyValuation::calculate(state, session(), close(), {bar(100), bar(100)}),
                 ValuationError);
    EXPECT_THROW((void)DailyValuation::calculate(state, session(), close(), {bar(100, 15)}),
                 ValuationError);
    EXPECT_THROW((void)DailyValuation::calculate(state, session(), close(15), {bar(100)}),
                 ValuationError);
    const auto valid =
        DailyValuation::calculate(state, session(), close(), {bar(100), bar(900, 14, "SPY")});
    EXPECT_EQ(valid.totalValue(), money(1000));
    ASSERT_EQ(valid.marks().size(), 1U);
    EXPECT_EQ(valid.marks()[0].symbol, symbol());
}

TEST(DailyValuation, RejectsFutureLedgerAndInconsistentPreviousValuation) {
    auto account = funded();
    const auto first = DailyValuation::calculate(account.snapshot(), session(), close(), {});
    EXPECT_THROW(
        (void)DailyValuation::calculate(account.snapshot(), session(), close(), {}, &first),
        ValuationError);
    EXPECT_THROW((void)DailyValuation::calculate(funded(1000, 2).snapshot(), session(15), close(15),
                                                 {}, &first),
                 ValuationError);
    EXPECT_THROW((void)DailyValuation::calculate(funded(2000).snapshot(), session(15), close(15),
                                                 {}, &first),
                 ValuationError);
    ASSERT_TRUE(account.applyTransaction(event(1, OrderSide::Buy, 2, 100, 0, 15)));
    EXPECT_THROW(
        (void)DailyValuation::calculate(account.snapshot(), session(), close(), {bar(100)}),
        ValuationError);
    const auto traded = DailyValuation::calculate(account.snapshot(), session(15), close(15),
                                                  {bar(100, 15)}, &first);
    EXPECT_THROW(
        (void)DailyValuation::calculate(funded().snapshot(), session(16), close(16), {}, &traded),
        ValuationError);
}

TEST(DailyValuation, MarksAndLedgerReplayGiveDeterministicResults) {
    auto account = funded();
    ASSERT_TRUE(account.applyTransaction(event(1, OrderSide::Buy, 2, 100, 0, 14, "ZZZ")));
    ASSERT_TRUE(account.applyTransaction(event(2, OrderSide::Buy, 1, 200, 0, 14, "AAA")));
    std::vector<PriceBar> bars{bar(110, 14, "ZZZ"), bar(210, 14, "AAA")};
    const auto first = DailyValuation::calculate(account.snapshot(), session(), close(), bars);
    std::reverse(bars.begin(), bars.end());
    const auto replay =
        Portfolio::replay(account.id(), account.startingCash(), account.transactionHistory());
    ASSERT_TRUE(replay);
    const auto second = DailyValuation::calculate(replay->snapshot(), session(), close(), bars);
    EXPECT_EQ(first.totalValue(), money(1030));
    EXPECT_EQ(first.totalValue(), second.totalValue());
    EXPECT_EQ(first.cumulativeReturn(), second.cumulativeReturn());
    EXPECT_EQ(first.ledgerSequence(), second.ledgerSequence());
    ASSERT_EQ(second.marks().size(), 2U);
    EXPECT_EQ(second.marks()[0].symbol, symbol("AAA"));
    EXPECT_EQ(second.marks()[1].symbol, symbol("ZZZ"));
}

TEST(DailyValuation, UnrepresentableValuationsAndReturnsFailExplicitly) {
    auto account = funded();
    ASSERT_TRUE(account.applyTransaction(event(1, OrderSide::Buy, 2, 100)));
    EXPECT_THROW((void)DailyValuation::calculate(account.snapshot(), session(), close(),
                                                 {bar(std::numeric_limits<double>::max())}),
                 ValuationError);
    auto tiny = funded(std::numeric_limits<double>::min());
    ASSERT_TRUE(
        tiny.applyTransaction(event(1, OrderSide::Buy, 1, std::numeric_limits<double>::min())));
    EXPECT_THROW((void)DailyValuation::calculate(tiny.snapshot(), session(), close(), {bar(100)}),
                 ValuationError);
}
TEST(DailyValuation, RejectsLateAndDivergentHistoricalPrefixes) {
    auto original = funded();
    const auto cash_only = DailyValuation::calculate(original.snapshot(), session(), close(), {});
    ASSERT_TRUE(original.applyTransaction(event(1, OrderSide::Buy, 2, 100)));
    EXPECT_THROW((void)DailyValuation::calculate(original.snapshot(), session(15), close(15),
                                                 {bar(100, 15)}, &cash_only),
                 ValuationError);
    const auto traded =
        DailyValuation::calculate(original.snapshot(), session(), close(), {bar(100)});
    auto different = funded();
    ASSERT_TRUE(different.applyTransaction(event(1, OrderSide::Buy, 2, 110)));
    EXPECT_THROW((void)DailyValuation::calculate(different.snapshot(), session(15), close(15),
                                                 {bar(100, 15)}, &traded),
                 ValuationError);
}
}  // namespace
