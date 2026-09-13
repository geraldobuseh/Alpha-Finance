#include "simulation/paper_portfolio_pair.hpp"

// Verify the public setup header without preceding test includes.
#include <gtest/gtest.h>

#include <limits>
#include <type_traits>

#include "strategy/spy_buy_and_hold.hpp"

namespace {
using namespace pql;
Money money(double value) { return Money::create(value).value(); }
Price price(double value) { return Price::create(value).value(); }
PortfolioId id(std::int64_t value) { return PortfolioId::create(value).value(); }
OrderId orderId(std::int64_t value) { return OrderId::create(value).value(); }
Timestamp time(std::int64_t value = 10) {
    return Timestamp{Timestamp::Value{std::chrono::milliseconds{value}}};
}
MarketState quote(double value = 100, std::int64_t tick = 10, const char* ticker = "SPY") {
    return MarketState{Symbol::create(ticker).value(), price(value), time(tick)};
}
TradingCosts freeTrading() { return TradingCosts{money(0), 0}; }
std::optional<PaperPortfolioPair> setup(double value = 100, TradingCosts costs = freeTrading()) {
    return PaperPortfolioPair::create(id(1), id(2), orderId(1), quote(value), costs);
}
Order paperOrder(std::int64_t tick = 10) {
    return Order::create_market(orderId(1), Symbol::create("SPY").value(), OrderSide::Buy,
                                Quantity::create(1).value(), time(tick))
        .value();
}
static_assert(!std::is_default_constructible_v<PaperPortfolioPair>);
static_assert(!std::is_copy_assignable_v<PaperPortfolioPair>);
static_assert(
    std::is_same_v<decltype(std::declval<PaperPortfolioPair>().benchmarkState()), PortfolioState>);

TEST(SpyBenchmark, EachPairStartsEquallyFundedAndImmediatelyInvestsTheControl) {
    for (std::int64_t base : {1, 10}) {
        auto pair =
            PaperPortfolioPair::create(id(base), id(base + 1), orderId(1), quote(), freeTrading());
        ASSERT_TRUE(pair);
        const auto paper = pair->paperState();
        const auto control = pair->benchmarkState();
        EXPECT_EQ(paper.id(), id(base));
        EXPECT_EQ(control.id(), id(base + 1));
        EXPECT_EQ(paper.startingCash(), money(1000));
        EXPECT_EQ(control.startingCash(), money(1000));
        EXPECT_EQ(paper.cashBalance(), money(1000));
        EXPECT_TRUE(paper.positions().empty());
        EXPECT_TRUE(paper.transactionHistory().empty());
        EXPECT_EQ(control.cashBalance(), money(0));
        ASSERT_EQ(control.positions().size(), 1U);
        EXPECT_EQ(control.positions()[0].symbol(), Symbol::create("SPY").value());
        EXPECT_DOUBLE_EQ(control.positions()[0].quantity().value(), 10);
        ASSERT_EQ(control.transactionHistory().size(), 1U);
        EXPECT_EQ(control.transactionHistory()[0].side(), OrderSide::Buy);
        EXPECT_EQ(control.transactionHistory()[0].timestamp(), pair->startTime());
    }
}

TEST(SpyBenchmark, ControlIsHeldAcrossPriceChangesWhilePaperRemainsIndependent) {
    auto pair = setup();
    ASSERT_TRUE(pair);
    const auto original = pair->benchmarkState();
    SpyBuyAndHold strategy{orderId(9), freeTrading()};
    Strategy& interface = strategy;
    EXPECT_EQ(interface.name(), "SPY Buy-and-Hold");
    for (double mark : {110.0, 90.0, 120.0}) {
        const auto market = quote(mark, 20);
        EXPECT_TRUE(interface.generateOrders(market, pair->benchmarkState()).empty());
        EXPECT_EQ(pair->benchmarkState().totalValue({{market.symbol(), market.price()}}),
                  money(mark * 10));
        EXPECT_EQ(pair->paperState().totalValue({{market.symbol(), market.price()}}), money(1000));
        EXPECT_EQ(pair->benchmarkState(), original);
    }
    ASSERT_TRUE(pair->executePaper(paperOrder(), quote()).isFilled());
    EXPECT_EQ(pair->paperState().cashBalance(), money(900));
    EXPECT_EQ(pair->benchmarkState(), original);
    // Reconstructing the strategy cannot cause another buy.
    SpyBuyAndHold restarted{orderId(20), freeTrading()};
    EXPECT_TRUE(restarted.generateOrders(quote(), original).empty());
}

TEST(SpyBenchmark, FractionalSizingIncludesSharedCommissionAndSlippage) {
    auto pair = setup(100, TradingCosts{money(2), 100});
    ASSERT_TRUE(pair);
    const auto control = pair->benchmarkState();
    const auto& transaction = control.transactionHistory().front();
    EXPECT_EQ(transaction.price(), price(101));
    EXPECT_EQ(transaction.fees(), money(2));
    EXPECT_NEAR(transaction.quantity().value(), 998.0 / 101.0, 1e-13);
    EXPECT_GE(control.cashBalance().value(), 0);
    EXPECT_LT(control.cashBalance().value(), 1e-10);
    const auto receipt = pair->executePaper(paperOrder(), quote());
    ASSERT_TRUE(receipt.isFilled());
    EXPECT_EQ(receipt.fill()->price, transaction.price());
    EXPECT_EQ(receipt.fill()->fees, transaction.fees());
    EXPECT_EQ(pair->paperState().cashBalance(), money(897));
    const auto replay =
        Portfolio::replay(control.id(), control.startingCash(), control.transactionHistory());
    ASSERT_TRUE(replay);
    EXPECT_EQ(replay->snapshot(), control);
    auto exact = setup(99, TradingCosts{money(10), 0});
    ASSERT_TRUE(exact);
    const auto exact_control = exact->benchmarkState();
    EXPECT_DOUBLE_EQ(exact_control.positions()[0].quantity().value(), 10);
    EXPECT_EQ(exact_control.positions()[0].average_cost(), price(100));
    EXPECT_EQ(exact_control.totalValue({{quote().symbol(), price(109)}}), money(1090));
}

TEST(SpyBenchmark, FractionalAndRoundedDownOrdersFitAvailableCash) {
    ASSERT_GT((1000.0 / 0.21) * 0.21, 1000.0);
    for (double value : {0.21, 300.0, 1500.0}) {
        auto pair = setup(value);
        ASSERT_TRUE(pair) << value;
        const auto control = pair->benchmarkState();
        const double units = control.positions()[0].quantity().value();
        EXPECT_GT(units, 0);
        EXPECT_LE(units * value, 1000);
        EXPECT_NEAR(units, 1000 / value, 1e-10);
        EXPECT_GE(control.cashBalance().value(), 0);
        EXPECT_LT(control.cashBalance().value(), 1e-10);
        if (value == 0.21) {
            EXPECT_LT(units, 1000 / value);
        }
    }
}

TEST(SpyBenchmark, SetupFailurePublishesNoCashOnlyControl) {
    EXPECT_FALSE(PaperPortfolioPair::create(id(1), id(1), orderId(1), quote(), freeTrading()));
    EXPECT_FALSE(
        PaperPortfolioPair::create(id(1), id(2), orderId(1), quote(100, 10, "QQQ"), freeTrading()));
    EXPECT_FALSE(
        PaperPortfolioPair::create(id(1), id(2), orderId(1), quote(100, 10, "spy"), freeTrading()));
    for (double fee : {1000.0, 1001.0}) EXPECT_FALSE(setup(100, TradingCosts{money(fee), 0}));
    EXPECT_FALSE(setup(std::numeric_limits<double>::max(), TradingCosts{money(0), 100}));
    EXPECT_FALSE(setup(std::numeric_limits<double>::denorm_min()));
    EXPECT_FALSE(setup(100, TradingCosts{money(0), 1e-15}));
    EXPECT_FALSE(setup(100, TradingCosts{money(std::numeric_limits<double>::min()), 0}));
}

TEST(SpyBenchmark, ProposalsDoNotMutateAndWaitForSpy) {
    auto portfolio = Portfolio::create(id(1), money(1000)).value();
    const auto before = portfolio.snapshot();
    SpyBuyAndHold strategy{orderId(1), freeTrading()};
    EXPECT_TRUE(strategy.generateOrders(quote(100, 10, "AAPL"), before).empty());
    const auto proposals = strategy.generateOrders(quote(), before);
    ASSERT_EQ(proposals.size(), 1U);
    EXPECT_EQ(strategy.generateOrders(quote(), before), proposals);
    EXPECT_EQ(portfolio.snapshot(), before);
    FakeBroker broker{portfolio, freeTrading()};
    ASSERT_TRUE(broker.execute(proposals[0], quote()).isFilled());
    EXPECT_TRUE(strategy.generateOrders(quote(), portfolio.snapshot()).empty());
}

TEST(SpyBenchmark, StartBoundaryCopiesAndReplayPreserveIndependentAccounts) {
    auto pair = setup();
    ASSERT_TRUE(pair);
    const auto before = pair->paperState();
    EXPECT_EQ(pair->executePaper(paperOrder(9), quote()).rejectionReason(),
              ExecutionRejection::InvalidTimestamp);
    EXPECT_EQ(pair->executePaper(paperOrder(), quote(100, 9)).rejectionReason(),
              ExecutionRejection::InvalidTimestamp);
    EXPECT_EQ(pair->paperState(), before);
    auto copied = *pair;
    ASSERT_TRUE(copied.executePaper(paperOrder(), quote()).isFilled());
    EXPECT_EQ(pair->paperState(), before);
    EXPECT_EQ(pair->benchmarkState(), copied.benchmarkState());
    const auto fresh = setup();
    ASSERT_TRUE(fresh);
    EXPECT_EQ(fresh->benchmarkState(), pair->benchmarkState());
}

}  // namespace
