#include "strategy/dollar_cost_averaging.hpp"

// Check the public header before test dependencies.
#include <gtest/gtest.h>

#include <limits>

#include "simulation/paper_portfolio_pair.hpp"

namespace {
using namespace pql;
Money money(double value) { return Money::create(value).value(); }
OrderId oid(std::int64_t value) { return OrderId::create(value).value(); }
Portfolio funded(double cash = 1000) {
    return Portfolio::create(PortfolioId::create(1).value(), money(cash)).value();
}
Timestamp tick(int days_after_monday = 0, int hours = 0) {
    return Timestamp{std::chrono::sys_days{std::chrono::year{2026} / 9 / 14} +
                     std::chrono::days{days_after_monday} + std::chrono::hours{hours}};
}
MarketState market(int day = 0, double price = 100, const char* symbol = "SPY", int hours = 0) {
    return MarketState{Symbol::create(symbol).value(), Price::create(price).value(),
                       tick(day, hours)};
}
TradingCosts freeCosts() { return TradingCosts{money(0), 0}; }

TEST(DollarCostAveraging, BuysTwentyFiveOnlyOnUtcMonday) {
    auto portfolio = funded();
    DollarCostAveraging strategy{freeCosts()};
    Strategy& interface = strategy;
    EXPECT_EQ(interface.name(), "SPY Weekly Dollar-Cost Averaging");
    const auto before = portfolio.snapshot();
    for (int day = 0; day < 7; ++day) {
        const auto orders = interface.generateOrders(market(day), before);
        if (day == 0) {
            ASSERT_EQ(orders.size(), 1U);
            EXPECT_EQ(orders[0].side(), OrderSide::Buy);
            EXPECT_EQ(orders[0].symbol(), market().symbol());
            EXPECT_DOUBLE_EQ(orders[0].quantity().value(), 0.25);
            EXPECT_EQ(orders[0].timestamp(), tick());
        } else {
            EXPECT_TRUE(orders.empty());
        }
    }
    EXPECT_TRUE(interface.generateOrders(market(0, 100, "QQQ"), before).empty());
    EXPECT_TRUE(interface.generateOrders(market(0, 100, "spy"), before).empty());
    EXPECT_EQ(portfolio.snapshot(), before);
    EXPECT_TRUE(interface.generateOrders(market(1), before).empty());  // No catch-up.
}

TEST(DollarCostAveraging, RepeatedAndRecreatedCallsUseLedgerAndNewWeeksAddShares) {
    auto portfolio = funded();
    const TradingCosts costs{money(1), 0};
    DollarCostAveraging strategy{costs};
    FakeBroker broker{portfolio, costs};
    const auto initial = portfolio.snapshot();
    const auto first = strategy.generateOrders(market(0, 12), initial);
    ASSERT_EQ(first.size(), 1U);
    EXPECT_EQ(strategy.generateOrders(market(0, 12), initial), first);
    ASSERT_TRUE(broker.execute(first[0], market(0, 12)).isFilled());
    EXPECT_EQ(portfolio.cashBalance(), money(975));
    EXPECT_DOUBLE_EQ(portfolio.positions()[0].quantity().value(), 2);
    EXPECT_TRUE(strategy.generateOrders(market(0, 12, "SPY", 23), portfolio.snapshot()).empty());
    DollarCostAveraging restarted{costs};
    EXPECT_TRUE(restarted.generateOrders(market(0, 12), portfolio.snapshot()).empty());
    const auto duplicate = broker.execute(first[0], market(0, 12));
    EXPECT_EQ(duplicate.rejectionReason(), ExecutionRejection::DuplicateOrder);
    const auto next = restarted.generateOrders(market(7, 24), portfolio.snapshot());
    ASSERT_EQ(next.size(), 1U);
    EXPECT_EQ(next[0].id(), oid(2));
    ASSERT_TRUE(broker.execute(next[0], market(7, 24)).isFilled());
    EXPECT_EQ(portfolio.cashBalance(), money(950));
    EXPECT_DOUBLE_EQ(portfolio.positions()[0].quantity().value(), 3);
    EXPECT_DOUBLE_EQ(portfolio.positions()[0].average_cost()->value(), 50.0 / 3.0);
    EXPECT_EQ(portfolio.totalValue({{market().symbol(), Price::create(24).value()}}), money(1022));
    const auto replay =
        Portfolio::replay(portfolio.id(), portfolio.startingCash(), portfolio.transactionHistory());
    ASSERT_TRUE(replay);
    EXPECT_EQ(replay->snapshot(), portfolio.snapshot());
}

TEST(DollarCostAveraging, BudgetIncludesCostsAndCashRemainderIsNotInvested) {
    const TradingCosts costs{money(1), 100};
    DollarCostAveraging strategy{costs};
    auto poor = funded(24.99);
    EXPECT_TRUE(strategy.generateOrders(market(), poor.snapshot()).empty());
    auto exact = funded(25);
    const auto orders = strategy.generateOrders(market(), exact.snapshot());
    ASSERT_EQ(orders.size(), 1U);
    EXPECT_NEAR(orders[0].quantity().value(), 24.0 / 101.0, 1e-15);
    FakeBroker broker{exact, costs};
    ASSERT_TRUE(broker.execute(orders[0], market()).isFilled());
    EXPECT_GE(exact.cashBalance().value(), 0);
    EXPECT_LT(exact.cashBalance().value(), 1e-12);
    auto full = funded();
    for (double fee : {25.0, 26.0}) {
        DollarCostAveraging too_costly{TradingCosts{money(fee), 0}};
        EXPECT_TRUE(too_costly.generateOrders(market(), full.snapshot()).empty());
    }
    DollarCostAveraging unrepresentable{TradingCosts{money(0), 1e-15}};
    EXPECT_TRUE(unrepresentable.generateOrders(market(), full.snapshot()).empty());
}

TEST(DollarCostAveraging, RejectedProposalCanRetryWithoutAdvancingScheduleOrId) {
    auto portfolio = funded(25);
    DollarCostAveraging strategy{freeCosts()};
    const auto before = portfolio.snapshot();
    const auto proposal = strategy.generateOrders(market(), before);
    ASSERT_EQ(proposal.size(), 1U);
    FakeBroker expensive{portfolio, money(1)};
    EXPECT_FALSE(expensive.execute(proposal[0], market()).isFilled());
    EXPECT_EQ(portfolio.snapshot(), before);
    EXPECT_EQ(strategy.generateOrders(market(), portfolio.snapshot()), proposal);
    FakeBroker correct{portfolio, freeCosts()};
    EXPECT_TRUE(correct.execute(proposal[0], market()).isFilled());
}

TEST(DollarCostAveraging, OrderIdsRespectGapsAndStopBeforeOverflow) {
    for (std::int64_t previous : {std::int64_t{70}, std::numeric_limits<std::int64_t>::max()}) {
        auto portfolio = funded();
        const auto quote = market(-7);
        const auto request = Order::create_market(oid(previous), quote.symbol(), OrderSide::Buy,
                                                  Quantity::create(0.25).value(), quote.timestamp())
                                 .value();
        FakeBroker broker{portfolio, freeCosts()};
        ASSERT_TRUE(broker.execute(request, quote).isFilled());
        DollarCostAveraging strategy{freeCosts()};
        const auto orders = strategy.generateOrders(market(), portfolio.snapshot());
        if (previous == std::numeric_limits<std::int64_t>::max()) {
            EXPECT_TRUE(orders.empty());
        } else {
            ASSERT_EQ(orders.size(), 1U);
            EXPECT_EQ(orders[0].id(), oid(71));
        }
    }
}

TEST(DollarCostAveraging, UtcBoundariesUseFloorAndFutureHistoryIsRejected) {
    auto portfolio = funded();
    DollarCostAveraging strategy{freeCosts()};
    EXPECT_TRUE(strategy.generateOrders(market(-1, 100, "SPY", 23), portfolio.snapshot()).empty());
    EXPECT_EQ(strategy.generateOrders(market(), portfolio.snapshot()).size(), 1U);
    EXPECT_EQ(strategy.generateOrders(market(0, 100, "SPY", 23), portfolio.snapshot()).size(), 1U);
    EXPECT_TRUE(strategy.generateOrders(market(1), portfolio.snapshot()).empty());
    const auto pre_epoch_day = std::chrono::sys_days{std::chrono::year{1969} / 12 / 29};  // Monday.
    const MarketState ancient{market().symbol(), market().price(), Timestamp{pre_epoch_day}};
    EXPECT_EQ(strategy.generateOrders(ancient, portfolio.snapshot()).size(), 1U);
    const MarketState sunday{market().symbol(), market().price(),
                             Timestamp{pre_epoch_day - std::chrono::milliseconds{1}}};
    EXPECT_TRUE(strategy.generateOrders(sunday, portfolio.snapshot()).empty());
    FakeBroker broker{portfolio, freeCosts()};
    const auto future = strategy.generateOrders(market(7), portfolio.snapshot());
    ASSERT_EQ(future.size(), 1U);
    ASSERT_TRUE(broker.execute(future[0], market(7)).isFilled());
    EXPECT_THROW((void)strategy.generateOrders(market(), portfolio.snapshot()),
                 std::invalid_argument);
}

TEST(DollarCostAveraging, PrefundedBaselineRunsAgainstHeldControlWithoutDeposits) {
    auto pair =
        PaperPortfolioPair::create(PortfolioId::create(1).value(), PortfolioId::create(2).value(),
                                   oid(1), market(), freeCosts());
    ASSERT_TRUE(pair);
    const auto control = pair->benchmarkState();
    DollarCostAveraging strategy{freeCosts()};
    for (int week = 0; week <= 40; ++week) {
        const auto quote = market(week * 7);
        const auto orders = strategy.generateOrders(quote, pair->paperState());
        if (week < 40) {
            ASSERT_EQ(orders.size(), 1U);
            EXPECT_TRUE(pair->executePaper(orders[0], quote).isFilled());
        } else {
            EXPECT_TRUE(orders.empty());
        }
        EXPECT_EQ(pair->benchmarkState(), control);
    }
    const auto paper = pair->paperState();
    EXPECT_EQ(paper.cashBalance(), money(0));
    EXPECT_EQ(paper.transactionHistory().size(), 40U);
    EXPECT_DOUBLE_EQ(paper.positions()[0].quantity().value(), 10);
    EXPECT_EQ(paper.startingCash(), money(1000));
}

}  // namespace
