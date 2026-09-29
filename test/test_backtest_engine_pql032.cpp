#include <gtest/gtest.h>

#include <algorithm>
#include <type_traits>

#include "simulation/backtest_engine.hpp"

namespace {
using namespace pql;

Money money(double value) { return Money::create(value).value(); }
Price price(double value) { return Price::create(value).value(); }
Quantity quantity(double value) { return Quantity::create(value).value(); }
Symbol symbol() { return Symbol::create("AAPL").value(); }
PortfolioId portfolioId() { return PortfolioId::create(1).value(); }
OrderId orderId(std::int64_t value) { return OrderId::create(value).value(); }

Date session(int day) { return Date::create(2026, 1, static_cast<unsigned>(day)).value(); }
Timestamp close(int day) {
    return Timestamp{std::chrono::sys_days{session(day).value()} + std::chrono::hours{21}};
}

PriceBar bar(int day, double close_price) {
    return PriceBar::create(symbol(), session(day), price(close_price), price(close_price),
                            price(close_price), price(close_price), quantity(1000))
        .value();
}

BacktestTradingDay tradingDay(int day, double market_price, double close_price) {
    return BacktestTradingDay{session(day),
                              close(day),
                              MarketState{symbol(), price(market_price), close(day)},
                              {bar(day, close_price)}};
}

Portfolio portfolio(double cash = 1000) {
    return Portfolio::create(portfolioId(), money(cash)).value();
}

class BuyThenSell final : public Strategy {
   public:
    std::vector<Order> generateOrders(const MarketState& market,
                                      const PortfolioState& state) override {
        const auto holding =
            std::find_if(state.positions().begin(), state.positions().end(),
                         [](const Position& position) { return position.symbol() == symbol(); });
        if (holding == state.positions().end() || holding->quantity().value() == 0.0) {
            if (state.cashBalance().value() >= 1000.0) {
                return {*Order::create_market(orderId(1), symbol(), OrderSide::Buy, quantity(10),
                                              market.timestamp())};
            }
            return {};
        }
        return {*Order::create_market(orderId(2), symbol(), OrderSide::Sell, quantity(10),
                                      market.timestamp())};
    }

    std::string name() const override { return "buy-then-sell"; }
};

TEST(BacktestEngine, InterfaceMatchesTicketShape) {
    static_assert(std::is_same_v<decltype(&BacktestEngine::run),
                                 BacktestResult (BacktestEngine::*)(
                                     Strategy&, HistoricalMarketData&, Portfolio) const>);
    static_assert(std::is_same_v<decltype(&run),
                                 BacktestResult (*)(Strategy&, HistoricalMarketData&, Portfolio)>);
}

TEST(BacktestEngine, RunsStrategyBrokerPortfolioAndSnapshotsEachTradingDay) {
    BuyThenSell strategy;
    HistoricalMarketData data{{tradingDay(2, 100, 100), tradingDay(5, 120, 120)}};

    const auto result = BacktestEngine{}.run(strategy, data, portfolio());

    EXPECT_EQ(result.strategy_name, "buy-then-sell");
    EXPECT_EQ(result.initial_portfolio.cashBalance(), money(1000));
    ASSERT_EQ(result.snapshots.size(), 2U);

    EXPECT_EQ(result.snapshots[0].session, session(2));
    ASSERT_EQ(result.snapshots[0].executions.size(), 1U);
    EXPECT_TRUE(result.snapshots[0].executions[0].isFilled());
    EXPECT_EQ(result.snapshots[0].portfolio.cashBalance(), money(0));
    ASSERT_EQ(result.snapshots[0].portfolio.positions().size(), 1U);
    EXPECT_EQ(result.snapshots[0].portfolio.positions()[0].quantity(), quantity(10));
    EXPECT_EQ(result.snapshots[0].valuation.cash(), money(0));
    EXPECT_EQ(result.snapshots[0].valuation.positionValue(), money(1000));
    EXPECT_EQ(result.snapshots[0].valuation.totalValue(), money(1000));
    EXPECT_FALSE(result.snapshots[0].valuation.dailyReturn());

    EXPECT_EQ(result.snapshots[1].session, session(5));
    ASSERT_EQ(result.snapshots[1].executions.size(), 1U);
    EXPECT_TRUE(result.snapshots[1].executions[0].isFilled());
    EXPECT_EQ(result.snapshots[1].portfolio.cashBalance(), money(1200));
    ASSERT_EQ(result.snapshots[1].portfolio.positions().size(), 1U);
    EXPECT_EQ(result.snapshots[1].portfolio.positions()[0].quantity(), quantity(0));
    EXPECT_EQ(result.snapshots[1].valuation.cash(), money(1200));
    EXPECT_EQ(result.snapshots[1].valuation.positionValue(), money(0));
    EXPECT_EQ(result.snapshots[1].valuation.totalValue(), money(1200));
    ASSERT_TRUE(result.snapshots[1].valuation.dailyReturn());
    EXPECT_NEAR(*result.snapshots[1].valuation.dailyReturn(), 0.2, 1e-15);
    ASSERT_TRUE(result.snapshots[1].valuation.cumulativeReturn());
    EXPECT_DOUBLE_EQ(*result.snapshots[1].valuation.cumulativeReturn(), 0.2);

    EXPECT_EQ(result.final_portfolio, result.snapshots.back().portfolio);
}

TEST(BacktestEngine, RepeatedRunsWithSameInputsAreDeterministic) {
    HistoricalMarketData first_data{{tradingDay(2, 100, 100), tradingDay(5, 120, 120)}};
    HistoricalMarketData second_data{{tradingDay(2, 100, 100), tradingDay(5, 120, 120)}};
    BuyThenSell first_strategy;
    BuyThenSell second_strategy;

    const auto first = run(first_strategy, first_data, portfolio());
    const auto second = run(second_strategy, second_data, portfolio());

    ASSERT_EQ(first.snapshots.size(), second.snapshots.size());
    EXPECT_EQ(first.initial_portfolio, second.initial_portfolio);
    EXPECT_EQ(first.final_portfolio, second.final_portfolio);
    for (std::size_t index = 0; index < first.snapshots.size(); ++index) {
        EXPECT_EQ(first.snapshots[index].portfolio, second.snapshots[index].portfolio);
        EXPECT_EQ(first.snapshots[index].valuation.totalValue(),
                  second.snapshots[index].valuation.totalValue());
        EXPECT_EQ(first.snapshots[index].executions.size(),
                  second.snapshots[index].executions.size());
        for (std::size_t fill = 0; fill < first.snapshots[index].executions.size(); ++fill) {
            EXPECT_EQ(first.snapshots[index].executions[fill].isFilled(),
                      second.snapshots[index].executions[fill].isFilled());
            EXPECT_EQ(first.snapshots[index].executions[fill].order_id(),
                      second.snapshots[index].executions[fill].order_id());
        }
    }
}

TEST(BacktestEngine, NoOrderDayStillSnapshotsPortfolioState) {
    BuyThenSell strategy;
    HistoricalMarketData data{{tradingDay(2, 100, 100)}};

    const auto result = run(strategy, data, portfolio(500));

    ASSERT_EQ(result.snapshots.size(), 1U);
    EXPECT_TRUE(result.snapshots[0].executions.empty());
    EXPECT_EQ(result.snapshots[0].portfolio.cashBalance(), money(500));
    EXPECT_EQ(result.snapshots[0].valuation.totalValue(), money(500));
}

TEST(BacktestEngine, RejectsAmbiguousHistoricalDataInsteadOfSortingOrRepairing) {
    BuyThenSell strategy;
    const auto reversed = [&] {
        HistoricalMarketData data{
            std::vector<BacktestTradingDay>{tradingDay(3, 120, 120), tradingDay(2, 100, 100)}};
        (void)run(strategy, data, portfolio());
    };
    EXPECT_THROW(reversed(), BacktestError);

    const auto mismatched_close = [&] {
        HistoricalMarketData data{std::vector<BacktestTradingDay>{BacktestTradingDay{
            session(2), close(3), MarketState{symbol(), price(100), close(2)}, {bar(2, 100)}}}};
        (void)run(strategy, data, portfolio());
    };
    EXPECT_THROW(mismatched_close(), BacktestError);

    const auto wrong_bar = [&] {
        HistoricalMarketData data{std::vector<BacktestTradingDay>{BacktestTradingDay{
            session(2), close(2), MarketState{symbol(), price(100), close(2)}, {bar(3, 100)}}}};
        (void)run(strategy, data, portfolio());
    };
    EXPECT_THROW(wrong_bar(), BacktestError);
}

class RejectedBuy final : public Strategy {
   public:
    std::vector<Order> generateOrders(const MarketState& market, const PortfolioState&) override {
        return {*Order::create_market(orderId(1), symbol(), OrderSide::Buy, quantity(20),
                                      market.timestamp())};
    }
    std::string name() const override { return "rejected-buy"; }
};

TEST(BacktestEngine, RejectedExecutionFailsTheBacktestWithoutSnapshottingPartialState) {
    RejectedBuy strategy;
    HistoricalMarketData data{{tradingDay(2, 100, 100)}};

    EXPECT_THROW((void)run(strategy, data, portfolio()), BacktestError);
}

class ObserveOnly final : public Strategy {
   public:
    int calls{};
    std::vector<Order> generateOrders(const MarketState&, const PortfolioState&) override {
        ++calls;
        return {};
    }
    std::string name() const override { return "observe-only"; }
};

TEST(BacktestEngine, ValidatesAllDaysBeforeCallingStrategy) {
    const auto rejects = [](std::vector<HistoricalTradingDay> days) {
        ObserveOnly strategy;
        HistoricalMarketData data{std::move(days)};
        EXPECT_THROW((void)run(strategy, data, portfolio()), BacktestError);
        EXPECT_EQ(strategy.calls, 0);
    };
    rejects({});
    rejects({tradingDay(2, 100, 100), tradingDay(2, 100, 100)});
    rejects({tradingDay(2, 100, 100), tradingDay(5, 100, 110)});
    rejects({HistoricalTradingDay{
        session(2), close(2), MarketState{symbol(), price(100), close(2)}, {}}});
    const auto msft = Symbol::create("MSFT").value();
    rejects({HistoricalTradingDay{
        session(2),
        close(2),
        MarketState{close(2), {{symbol(), price(100), {}}, {msft, price(100), {}}}},
        {bar(2, 100), bar(2, 100)}}});
    rejects({HistoricalTradingDay{
        session(2), close(2), MarketState{msft, price(100), close(2)}, {bar(2, 100)}}});
    rejects({HistoricalTradingDay{
        session(2), close(5), MarketState{symbol(), price(100), close(5)}, {bar(2, 100)}}});
}

TEST(BacktestEngine, RejectsFutureInitialLedgerBeforeCallingStrategy) {
    auto initial = portfolio();
    const auto order =
        *Order::create_market(orderId(99), symbol(), OrderSide::Buy, quantity(1), close(5));
    ASSERT_TRUE(initial.applyTrade(*Trade::create(order, price(100), close(5))));
    const auto before = initial.snapshot();
    ObserveOnly strategy;
    HistoricalMarketData data{{tradingDay(2, 100, 100)}};

    EXPECT_THROW((void)run(strategy, data, initial), BacktestError);
    EXPECT_EQ(strategy.calls, 0);
    EXPECT_EQ(initial.snapshot(), before);
}

TEST(BacktestEngine, PreservesPriorHoldingsAndLifetimeReturnBaseline) {
    auto initial = portfolio();
    const auto order =
        *Order::create_market(orderId(99), symbol(), OrderSide::Buy, quantity(1), close(1));
    ASSERT_TRUE(initial.applyTrade(*Trade::create(order, price(100), close(1))));
    const auto before = initial.snapshot();
    ObserveOnly strategy;
    HistoricalMarketData data{{tradingDay(2, 110, 110), tradingDay(5, 120, 120)}};

    const auto result = run(strategy, data, initial);

    EXPECT_EQ(initial.snapshot(), before);
    EXPECT_EQ(result.final_portfolio, before);
    ASSERT_EQ(result.snapshots.size(), 2U);
    EXPECT_EQ(result.snapshots[0].valuation.totalValue(), money(1010));
    EXPECT_EQ(result.snapshots[1].valuation.totalValue(), money(1020));
    ASSERT_TRUE(result.snapshots[1].valuation.cumulativeReturn());
    EXPECT_NEAR(*result.snapshots[1].valuation.cumulativeReturn(), 0.02, 1e-15);
    EXPECT_EQ(strategy.calls, 2);
}

TEST(BacktestEngine, MissingHeldAssetMarkFailsWithoutChangingCallerPortfolio) {
    auto initial = portfolio();
    const auto held_symbol = Symbol::create("MSFT").value();
    const auto order =
        *Order::create_market(orderId(99), held_symbol, OrderSide::Buy, quantity(1), close(1));
    ASSERT_TRUE(initial.applyTrade(*Trade::create(order, price(100), close(1))));
    const auto before = initial.snapshot();
    ObserveOnly strategy;
    HistoricalMarketData data{{tradingDay(2, 100, 100)}};

    EXPECT_THROW((void)run(strategy, data, initial), ValuationError);
    EXPECT_EQ(strategy.calls, 1);
    EXPECT_EQ(initial.snapshot(), before);
}

class FixedBuy final : public Strategy {
   public:
    std::vector<Order> generateOrders(const MarketState& market,
                                      const PortfolioState& state) override {
        if (!state.transactionHistory().empty()) return {};
        return {*Order::create_market(orderId(1), symbol(), OrderSide::Buy, quantity(2),
                                      market.timestamp())};
    }
    std::string name() const override { return "fixed-buy"; }
};

TEST(BacktestEngine, RecordsCostsAndReconstructableLedgerWithoutMutatingInputs) {
    FixedBuy strategy;
    HistoricalMarketData data{{tradingDay(2, 100, 100), tradingDay(5, 120, 120)}};
    const auto initial = portfolio();
    const auto before = initial.snapshot();
    const BacktestEngine engine{TradingCosts{money(1), 100}};

    const auto first = engine.run(strategy, data, initial);
    const auto second = engine.run(strategy, data, initial);

    EXPECT_EQ(initial.snapshot(), before);
    EXPECT_EQ(first.final_portfolio, second.final_portfolio);
    EXPECT_EQ(data.tradingDays()[0].market.price(), price(100));
    EXPECT_EQ(data.tradingDays()[0].closing_prices[0].close(), price(100));
    ASSERT_EQ(first.snapshots[0].executions.size(), 1U);
    const auto& fill = first.snapshots[0].executions[0].fill();
    ASSERT_TRUE(fill);
    EXPECT_EQ(fill->price, price(101));
    EXPECT_EQ(fill->fees, money(1));
    EXPECT_EQ(fill->timestamp, close(2));
    EXPECT_EQ(first.snapshots[0].valuation.totalValue(), money(997));
    EXPECT_EQ(first.snapshots[1].valuation.totalValue(), money(1037));
    for (const auto& snapshot : first.snapshots) {
        const auto replayed =
            Portfolio::replay(snapshot.portfolio.id(), snapshot.portfolio.startingCash(),
                              snapshot.portfolio.transactionHistory());
        ASSERT_TRUE(replayed);
        EXPECT_EQ(replayed->snapshot(), snapshot.portfolio);
    }
}

class FailingBatch final : public Strategy {
   public:
    std::vector<Order> generateOrders(const MarketState& market, const PortfolioState&) override {
        return {*Order::create_market(orderId(1), symbol(), OrderSide::Buy, quantity(1),
                                      market.timestamp()),
                *Order::create_market(orderId(2), symbol(), OrderSide::Buy, quantity(20),
                                      market.timestamp())};
    }
    std::string name() const override { return "failing-batch"; }
};

TEST(BacktestEngine, LaterRejectedLegLeavesCallerPortfolioRetryable) {
    FailingBatch strategy;
    HistoricalMarketData data{{tradingDay(2, 100, 100)}};
    const auto initial = portfolio();
    const auto before = initial.snapshot();
    EXPECT_THROW((void)run(strategy, data, initial), BacktestError);
    EXPECT_EQ(initial.snapshot(), before);
    FixedBuy retry;
    const auto result = run(retry, data, initial);
    EXPECT_EQ(result.final_portfolio.transactionHistory().size(), 1U);
    EXPECT_EQ(result.final_portfolio.cashBalance(), money(800));
}

TEST(BacktestEngine, HistoryCannotExposeCurrentOrFutureDailyBars) {
    EXPECT_THROW((MarketState{close(2), {{symbol(), price(100), {bar(2, 100)}}}}),
                 std::invalid_argument);
    EXPECT_THROW((MarketState{close(2), {{symbol(), price(100), {bar(5, 100)}}}}),
                 std::invalid_argument);
}

}  // namespace
