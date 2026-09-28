#include <gtest/gtest.h>

#include <cmath>
#include <limits>

#include "analytics/strategy_scorecard.hpp"

namespace {
using namespace pql;

Money money(double value) { return Money::create(value).value(); }
Price price(double value) { return Price::create(value).value(); }
Quantity quantity(double value) { return Quantity::create(value).value(); }
Symbol symbol() { return Symbol::create("AAPL").value(); }
PortfolioId portfolioId(std::int64_t value = 1) { return PortfolioId::create(value).value(); }
Timestamp timestamp(int day) {
    return Timestamp{std::chrono::sys_days{std::chrono::year{2026} / 1 / day}};
}
EquityPoint point(int day, double value) { return {timestamp(day), money(value)}; }

Transaction transaction(std::int64_t id, OrderSide side, double units, double fill, double fee,
                        int day) {
    const auto order = Order::create_market(OrderId::create(id).value(), symbol(), side,
                                            quantity(units), timestamp(day))
                           .value();
    const auto trade = Trade::create(order, price(fill), timestamp(day)).value();
    return Transaction::create(portfolioId(), trade, money(fee)).value();
}

Portfolio scoredPortfolio() {
    auto portfolio = Portfolio::create(portfolioId(), money(1000)).value();
    EXPECT_TRUE(portfolio.applyTransaction(transaction(1, OrderSide::Buy, 2, 100, 1, 2)));
    EXPECT_TRUE(portfolio.applyTransaction(transaction(2, OrderSide::Sell, 1, 120, 2, 3)));
    return portfolio;
}

TEST(StrategyScorecard, CalculatesEveryMetricFromAlignedHistory) {
    const auto portfolio = scoredPortfolio();
    const std::vector<EquityPoint> strategy{point(1, 1000), point(2, 1100), point(4, 990)};
    const std::vector<EquityPoint> spy{point(1, 1000), point(2, 1050), point(4, 1020)};

    const auto result =
        buildStrategyScorecard("synthetic", portfolio.snapshot(), strategy, spy, 252);

    EXPECT_EQ(result.strategy_name, "synthetic");
    EXPECT_EQ(result.starting_capital, money(1000));
    EXPECT_EQ(result.ending_capital, money(990));
    EXPECT_NEAR(result.total_return, -0.01, 1e-15);
    EXPECT_NEAR(result.spy_return, 0.02, 1e-15);
    EXPECT_NEAR(result.excess_return, -0.03, 1e-15);
    EXPECT_NEAR(result.max_drawdown.fraction, -0.10, 1e-15);
    EXPECT_EQ(result.max_drawdown.peak, point(2, 1100));
    EXPECT_EQ(result.max_drawdown.trough, point(4, 990));
    ASSERT_TRUE(result.volatility);
    EXPECT_NEAR(result.volatility->per_period, std::sqrt(0.02), 1e-15);
    ASSERT_TRUE(result.volatility->annualized);
    EXPECT_NEAR(*result.volatility->annualized, std::sqrt(0.02 * 252), 1e-14);
    ASSERT_TRUE(result.sharpe);
    EXPECT_NEAR(result.sharpe->per_period, 0.0, 1e-15);
    EXPECT_EQ(result.number_of_trades, 2U);
    EXPECT_NEAR(result.turnover, 320.0 / 1030.0, 1e-15);
    EXPECT_EQ(result.fees, money(3));
}

TEST(StrategyScorecard, NoTradesAndInsufficientRiskSamplesRemainExplicit) {
    const auto portfolio = Portfolio::create(portfolioId(), money(1000)).value();
    const std::vector<EquityPoint> strategy{point(1, 1000)};
    const std::vector<EquityPoint> spy{point(1, 500)};
    const auto result = buildStrategyScorecard("cash", portfolio.snapshot(), strategy, spy);
    EXPECT_DOUBLE_EQ(result.total_return, 0.0);
    EXPECT_DOUBLE_EQ(result.spy_return, 0.0);
    EXPECT_DOUBLE_EQ(result.excess_return, 0.0);
    EXPECT_DOUBLE_EQ(result.max_drawdown.fraction, 0.0);
    EXPECT_FALSE(result.volatility);
    EXPECT_FALSE(result.sharpe);
    EXPECT_EQ(result.number_of_trades, 0U);
    EXPECT_DOUBLE_EQ(result.turnover, 0.0);
    EXPECT_EQ(result.fees, money(0));
}

TEST(StrategyScorecard, PositiveExcessDoesNotRequirePositiveAbsoluteReturn) {
    auto portfolio = Portfolio::create(portfolioId(), money(1000)).value();
    ASSERT_TRUE(portfolio.applyTransaction(transaction(1, OrderSide::Buy, 10, 100, 0, 2)));
    const auto result =
        buildStrategyScorecard("less bad", portfolio.snapshot(), {point(1, 1000), point(3, 900)},
                               {point(1, 1000), point(3, 800)});
    EXPECT_DOUBLE_EQ(result.total_return, -0.1);
    EXPECT_DOUBLE_EQ(result.spy_return, -0.2);
    EXPECT_DOUBLE_EQ(result.excess_return, 0.1);
    EXPECT_FALSE(result.volatility);
    EXPECT_FALSE(result.sharpe);
}

TEST(StrategyScorecard, RejectsMisalignedOrInvalidReportingWindows) {
    const auto portfolio = Portfolio::create(portfolioId(), money(1000)).value();
    const std::vector<EquityPoint> strategy{point(1, 1000), point(2, 1000)};
    EXPECT_THROW((void)buildStrategyScorecard("", portfolio.snapshot(), strategy, strategy),
                 ScorecardError);
    EXPECT_THROW((void)buildStrategyScorecard("x", portfolio.snapshot(), {}, {}), ScorecardError);
    EXPECT_THROW(
        (void)buildStrategyScorecard("x", portfolio.snapshot(), strategy, {point(1, 1000)}),
        ScorecardError);
    EXPECT_THROW((void)buildStrategyScorecard("x", portfolio.snapshot(), strategy,
                                              {point(1, 1000), point(3, 1000)}),
                 ScorecardError);
    EXPECT_THROW(
        (void)buildStrategyScorecard("x", portfolio.snapshot(), {point(2, 1000), point(1, 1000)},
                                     {point(2, 1000), point(1, 1000)}),
        ScorecardError);
    EXPECT_THROW(
        (void)buildStrategyScorecard("x", portfolio.snapshot(), {point(1, 999)}, {point(1, 1000)}),
        ScorecardError);
    const auto zero = Portfolio::create(portfolioId(), money(0)).value();
    EXPECT_THROW(
        (void)buildStrategyScorecard("x", zero.snapshot(), {point(1, 0)}, {point(1, 1000)}),
        ScorecardError);
    EXPECT_THROW((void)buildStrategyScorecard("x", portfolio.snapshot(), strategy, strategy, 0),
                 ScorecardError);
    EXPECT_THROW((void)buildStrategyScorecard("x", portfolio.snapshot(),
                                              {point(1, 1000), point(2, 1000), point(3, 1000)},
                                              {point(1, 1000), point(2, -5), point(3, 1000)}),
                 ScorecardError);
}

TEST(StrategyScorecard, RejectsLedgerOutsideWindowAndUndefinedInternalReturn) {
    auto before = Portfolio::create(portfolioId(), money(1000)).value();
    ASSERT_TRUE(before.applyTransaction(transaction(1, OrderSide::Buy, 1, 100, 1, 1)));
    EXPECT_THROW(
        (void)buildStrategyScorecard("x", before.snapshot(), {point(1, 1000), point(2, 999)},
                                     {point(1, 1000), point(2, 1000)}),
        ScorecardError);
    auto after = Portfolio::create(portfolioId(), money(1000)).value();
    ASSERT_TRUE(after.applyTransaction(transaction(1, OrderSide::Buy, 1, 100, 1, 3)));
    EXPECT_THROW(
        (void)buildStrategyScorecard("x", after.snapshot(), {point(1, 1000), point(2, 999)},
                                     {point(1, 1000), point(2, 1000)}),
        ScorecardError);
    const auto cash = Portfolio::create(portfolioId(), money(1000)).value();
    EXPECT_THROW((void)buildStrategyScorecard("x", cash.snapshot(),
                                              {point(1, 1000), point(2, 0), point(3, 1)},
                                              {point(1, 1000), point(2, 1000), point(3, 1000)}),
                 ScorecardError);
}

TEST(StrategyScorecard, RejectsEquityThatContradictsReplayedLedger) {
    const auto cash = Portfolio::create(portfolioId(), money(1000)).value();
    EXPECT_THROW((void)buildStrategyScorecard("x", cash.snapshot(), {point(1, 1000), point(2, 900)},
                                              {point(1, 1000), point(2, 1000)}),
                 ScorecardError);

    auto open_position = Portfolio::create(portfolioId(), money(1000)).value();
    ASSERT_TRUE(open_position.applyTransaction(transaction(1, OrderSide::Buy, 1, 100, 0, 2)));
    EXPECT_THROW(
        (void)buildStrategyScorecard("x", open_position.snapshot(), {point(1, 1000), point(3, 899)},
                                     {point(1, 1000), point(3, 1000)}),
        ScorecardError);
}

TEST(StrategyScorecard, RepeatedEvaluationIsDeterministicAndDoesNotMutateInputs) {
    const auto portfolio = scoredPortfolio();
    const std::vector<EquityPoint> strategy{point(1, 1000), point(2, 1100), point(4, 990)};
    const std::vector<EquityPoint> spy{point(1, 1000), point(2, 1050), point(4, 1020)};
    const auto state = portfolio.snapshot();
    const auto strategy_before = strategy;
    const auto spy_before = spy;
    EXPECT_EQ(buildStrategyScorecard("synthetic", state, strategy, spy, 252),
              buildStrategyScorecard("synthetic", state, strategy, spy, 252));
    EXPECT_EQ(strategy, strategy_before);
    EXPECT_EQ(spy, spy_before);
    EXPECT_EQ(portfolio.snapshot(), state);
}

}  // namespace
