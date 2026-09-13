#include "execution/fake_broker.hpp"

// Public header must compile before test dependencies.
#include <gtest/gtest.h>

#include <limits>
#include <memory>

namespace {
using namespace pql;

Money money(double value) { return Money::create(value).value(); }
Price price(double value) { return Price::create(value).value(); }
Quantity quantity(double value) { return Quantity::create(value).value(); }
Symbol symbol(const char* value = "AAPL") { return Symbol::create(value).value(); }
Timestamp time(std::int64_t tick = 10) {
    return Timestamp{Timestamp::Value{std::chrono::milliseconds{tick}}};
}
Portfolio funded(double cash = 1000) {
    return Portfolio::create(PortfolioId::create(1).value(), money(cash)).value();
}
Order order(std::int64_t id, OrderSide side = OrderSide::Buy, double units = 2,
            const char* ticker = "AAPL", std::int64_t tick = 10) {
    return Order::create_market(OrderId::create(id).value(), symbol(ticker), side, quantity(units),
                                time(tick))
        .value();
}
MarketState market(double value = 100, std::int64_t tick = 10) {
    return MarketState{symbol(), price(value), time(tick)};
}

void expectRejected(Broker& broker, Portfolio& portfolio, const Order& request,
                    const MarketState& quote, ExecutionRejection reason) {
    const auto before = portfolio.snapshot();
    const auto result = broker.execute(request, quote);
    EXPECT_FALSE(result.isFilled());
    EXPECT_FALSE(result.fill());
    EXPECT_EQ(result.rejectionReason(), reason);
    EXPECT_EQ(result.portfolio_id(), portfolio.id());
    EXPECT_EQ(result.order_id(), request.id());
    EXPECT_EQ(portfolio.snapshot(), before);
}

TEST(FakeBroker, PolymorphicBuyUsesConfiguredQuoteAndRecordsFeeBeforePortfolioUpdate) {
    auto portfolio = funded();
    std::unique_ptr<Broker> broker = std::make_unique<FakeBroker>(portfolio, money(2));
    const auto request = order(1);
    const auto result = broker->execute(request, market(100, 20));
    ASSERT_TRUE(result.isFilled());
    ASSERT_TRUE(result.fill());
    EXPECT_FALSE(result.rejectionReason());
    EXPECT_EQ(result.order_id(), request.id());
    EXPECT_EQ(result.portfolio_id(), portfolio.id());
    EXPECT_EQ(result.fill()->side, OrderSide::Buy);
    EXPECT_EQ(result.fill()->quantity, quantity(2));
    EXPECT_EQ(result.fill()->price, price(100));
    EXPECT_EQ(result.fill()->fees, money(2));
    EXPECT_EQ(result.fill()->timestamp, time(20));
    EXPECT_EQ(portfolio.cashBalance(), money(798));
    ASSERT_EQ(portfolio.positions().size(), 1U);
    EXPECT_EQ(portfolio.positions()[0].quantity(), quantity(2));
    EXPECT_EQ(portfolio.positions()[0].average_cost(), price(101));
    ASSERT_EQ(portfolio.transactionHistory().size(), 1U);
    const auto& event = portfolio.transactionHistory()[0];
    EXPECT_EQ(event.symbol(), request.symbol());
    EXPECT_EQ(event.price(), result.fill()->price);
    EXPECT_EQ(event.fees(), result.fill()->fees);
    EXPECT_EQ(event.timestamp(), result.fill()->timestamp);
    EXPECT_EQ(request.status(), OrderStatus::Pending);
}

TEST(FakeBroker, BuysPartialSalesAndLiquidationReplayWithFees) {
    auto portfolio = funded();
    FakeBroker broker{portfolio, money(2)};
    ASSERT_TRUE(broker.execute(order(1), market()).isFilled());
    ASSERT_TRUE(broker.execute(order(2, OrderSide::Sell, 1), market()).isFilled());
    EXPECT_EQ(portfolio.cashBalance(), money(896));
    EXPECT_EQ(portfolio.positions()[0].quantity(), quantity(1));
    EXPECT_EQ(portfolio.positions()[0].realized_pnl(), money(-3));
    const auto sale = broker.execute(order(3, OrderSide::Sell, 1), market());
    ASSERT_TRUE(sale.isFilled());
    EXPECT_EQ(sale.fill()->side, OrderSide::Sell);
    EXPECT_EQ(portfolio.cashBalance(), money(994));
    EXPECT_EQ(portfolio.positions()[0].quantity(), quantity(0));
    EXPECT_FALSE(portfolio.positions()[0].average_cost());
    EXPECT_EQ(portfolio.positions()[0].realized_pnl(), money(-6));
    ASSERT_EQ(portfolio.transactionHistory().size(), 3U);
    const auto replay =
        Portfolio::replay(portfolio.id(), portfolio.startingCash(), portfolio.transactionHistory());
    ASSERT_TRUE(replay);
    EXPECT_EQ(replay->snapshot(), portfolio.snapshot());
}

TEST(FakeBroker, QuoteChangesDriveFillPriceAndResultsAreDeterministic) {
    auto first = funded();
    auto second = funded();
    FakeBroker a{first, money(2)};
    FakeBroker b{second, money(2)};
    for (Broker* broker : {static_cast<Broker*>(&a), static_cast<Broker*>(&b)}) {
        ASSERT_TRUE(broker->execute(order(1), market()).isFilled());
        auto sale = broker->execute(order(2, OrderSide::Sell, 1), market(120, 20));
        ASSERT_TRUE(sale.isFilled());
        EXPECT_EQ(sale.fill()->price, price(120));
    }
    EXPECT_EQ(first.snapshot(), second.snapshot());
    EXPECT_EQ(first.cashBalance(), money(916));
    EXPECT_EQ(first.positions()[0].realized_pnl(), money(17));
    EXPECT_EQ(first.totalValue({{symbol(), price(120)}}), money(1036));
}

TEST(FakeBroker, CashChecksIncludeFeeAndAllowExactFunding) {
    for (double cash : {199.0, 200.0, 201.0}) {
        auto portfolio = funded(cash);
        FakeBroker broker{portfolio, money(2)};
        expectRejected(broker, portfolio, order(1), market(), ExecutionRejection::InsufficientCash);
        // A rejected attempt neither consumes the order ID nor charges a fee.
        ASSERT_TRUE(broker.execute(order(1, OrderSide::Buy, 1), market()).isFilled());
        EXPECT_EQ(portfolio.cashBalance(), money(cash - 102));
        EXPECT_EQ(portfolio.transactionHistory().size(), 1U);
    }
    auto portfolio = funded(202);
    FakeBroker broker{portfolio, money(2)};
    EXPECT_TRUE(broker.execute(order(1), market()).isFilled());
    EXPECT_EQ(portfolio.cashBalance(), money(0));
}

TEST(FakeBroker, SellRequiresExistingSufficientShares) {
    auto portfolio = funded();
    FakeBroker broker{portfolio, money(2)};
    expectRejected(broker, portfolio, order(1, OrderSide::Sell), market(),
                   ExecutionRejection::InsufficientShares);
    ASSERT_TRUE(broker.execute(order(1), market()).isFilled());
    expectRejected(broker, portfolio, order(2, OrderSide::Sell, 3), market(),
                   ExecutionRejection::InsufficientShares);
    ASSERT_TRUE(broker.execute(order(2, OrderSide::Sell), market()).isFilled());
    expectRejected(broker, portfolio, order(3, OrderSide::Sell, 1), market(),
                   ExecutionRejection::InsufficientShares);
}

TEST(FakeBroker, InvalidInputIsRejectedAtTypedBoundaryAndUnknownQuoteDoesNotMutate) {
    auto portfolio = funded();
    FakeBroker broker{portfolio, money(2)};
    const auto before = portfolio.snapshot();
    EXPECT_FALSE(Order::create_market(OrderId::create(1).value(), symbol(), OrderSide::Buy,
                                      quantity(0), time()));
    for (const char* text : {"", " AAPL", "AAPL ", "AAP L", "AAPL!"}) {
        EXPECT_FALSE(Symbol::create(text));
    }
    EXPECT_EQ(portfolio.snapshot(), before);
    expectRejected(broker, portfolio, order(1, OrderSide::Buy, 1, "MSFT"), market(),
                   ExecutionRejection::UnknownSymbol);
    expectRejected(broker, portfolio, order(1, OrderSide::Buy, 1, "aapl"), market(),
                   ExecutionRejection::UnknownSymbol);
}

TEST(FakeBroker, DuplicateAndReversedTimesDoNotChargeAgain) {
    auto portfolio = funded();
    FakeBroker broker{portfolio, money(2)};
    expectRejected(broker, portfolio, order(1), market(100, 9),
                   ExecutionRejection::InvalidTimestamp);
    ASSERT_TRUE(broker.execute(order(1), market(100, 20)).isFilled());
    expectRejected(broker, portfolio, order(1), market(100, 20),
                   ExecutionRejection::DuplicateOrder);
    expectRejected(broker, portfolio, order(2), market(100, 19),
                   ExecutionRejection::InvalidTimestamp);
    FakeBroker replacement{portfolio, money(2)};
    expectRejected(replacement, portfolio, order(1), market(100, 20),
                   ExecutionRejection::DuplicateOrder);
    EXPECT_TRUE(replacement.execute(order(2), market(100, 20)).isFilled());
}

TEST(FakeBroker, ZeroFeeAndFractionalSharesAreSupportedNegativeFeeIsNot) {
    auto portfolio = funded(25);
    EXPECT_THROW((FakeBroker{portfolio, money(-1)}), std::invalid_argument);
    FakeBroker broker{portfolio, money(0)};
    const auto fill = broker.execute(order(1, OrderSide::Buy, 0.25), market());
    ASSERT_TRUE(fill.isFilled());
    EXPECT_EQ(fill.fill()->fees, money(0));
    EXPECT_EQ(portfolio.cashBalance(), money(0));
    EXPECT_EQ(portfolio.positions()[0].quantity(), quantity(0.25));
}

TEST(FakeBroker, SaleFeesMayConsumeProceedsButCannotOverdrawCash) {
    for (double sale_fee : {100.0, 101.0}) {
        for (double initial_cash : {100.0, 101.0}) {
            auto portfolio = funded(initial_cash);
            FakeBroker buy{portfolio, money(0)};
            ASSERT_TRUE(buy.execute(order(1, OrderSide::Buy, 1), market()).isFilled());
            FakeBroker sell{portfolio, money(sale_fee)};
            if (initial_cash == 100 && sale_fee == 101) {
                expectRejected(sell, portfolio, order(2, OrderSide::Sell, 1), market(),
                               ExecutionRejection::InsufficientCash);
            } else {
                EXPECT_TRUE(sell.execute(order(2, OrderSide::Sell, 1), market()).isFilled());
                EXPECT_EQ(portfolio.cashBalance(), money(initial_cash - sale_fee));
            }
        }
    }
}

TEST(FakeBroker, UnrepresentableAmountsAndLedgerNumericFailuresRemainAtomic) {
    auto portfolio = funded(std::numeric_limits<double>::max());
    FakeBroker broker{portfolio, money(0)};
    expectRejected(broker, portfolio, order(1, OrderSide::Buy, 2),
                   market(std::numeric_limits<double>::max()),
                   ExecutionRejection::InvalidArithmetic);
    expectRejected(broker, portfolio, order(1, OrderSide::Buy, std::numeric_limits<double>::min()),
                   market(std::numeric_limits<double>::min()),
                   ExecutionRejection::InvalidArithmetic);
    // Cash subtraction would erase the entire cost through rounding.
    expectRejected(broker, portfolio, order(1, OrderSide::Buy, 1), market(),
                   ExecutionRejection::PortfolioRejected);
    auto ordinary = funded();
    FakeBroker tiny_fee{ordinary, money(std::numeric_limits<double>::min())};
    expectRejected(tiny_fee, ordinary, order(1), market(), ExecutionRejection::PortfolioRejected);
}

TEST(FakeBroker, TradingCostsRejectInvalidConfiguration) {
    EXPECT_THROW((TradingCosts{money(-1), 0}), std::invalid_argument);
    for (double bps :
         {-1.0, 10000.0, 10001.0, std::numeric_limits<double>::infinity(),
          -std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()}) {
        EXPECT_THROW((TradingCosts{money(1), bps}), std::invalid_argument);
    }
    EXPECT_NO_THROW((TradingCosts{money(0), 0}));
    EXPECT_NO_THROW((TradingCosts{money(2), 9999}));
    const TradingCosts costs{money(1), 5};
    EXPECT_EQ(costs.commission(), money(1));
    EXPECT_DOUBLE_EQ(costs.slippageBasisPoints(), 5);
}

TEST(FakeBroker, FiveBasisPointsMatchesExampleAndCommissionIsNotSlippage) {
    auto portfolio = funded();
    FakeBroker broker{portfolio, TradingCosts{money(1), 5}};
    const auto quote = market();
    const auto buy = broker.execute(order(1), quote);
    ASSERT_TRUE(buy.isFilled());
    EXPECT_DOUBLE_EQ(buy.fill()->price.value(), 100.05);
    EXPECT_EQ(buy.fill()->fees, money(1));
    EXPECT_DOUBLE_EQ(portfolio.cashBalance().value(), 798.90);
    EXPECT_DOUBLE_EQ(portfolio.positions()[0].average_cost()->value(), 100.55);
    const auto sale = broker.execute(order(2, OrderSide::Sell), quote);
    ASSERT_TRUE(sale.isFilled());
    EXPECT_DOUBLE_EQ(sale.fill()->price.value(), 99.95);
    EXPECT_EQ(sale.fill()->fees, money(1));
    EXPECT_DOUBLE_EQ(portfolio.cashBalance().value(), 997.80);
    EXPECT_NEAR(portfolio.positions()[0].realized_pnl().value(), -2.20, 1e-12);
    EXPECT_EQ(quote.price(), price(100));
    EXPECT_EQ(portfolio.transactionHistory()[0].price(), buy.fill()->price);
    EXPECT_EQ(portfolio.transactionHistory()[1].price(), sale.fill()->price);
    const auto replay =
        Portfolio::replay(portfolio.id(), portfolio.startingCash(), portfolio.transactionHistory());
    ASSERT_TRUE(replay);
    EXPECT_EQ(replay->snapshot(), portfolio.snapshot());
}

TEST(FakeBroker, CostsAreIndependentConfigurableAndDeterministic) {
    for (double commission : {0.0, 2.0, 3.0}) {
        for (double bps : {0.0, 100.0}) {
            auto portfolio = funded();
            auto duplicate = funded();
            FakeBroker broker{portfolio, TradingCosts{money(commission), bps}};
            FakeBroker other{duplicate, TradingCosts{money(commission), bps}};
            for (Broker* target : {static_cast<Broker*>(&broker), static_cast<Broker*>(&other)}) {
                ASSERT_TRUE(target->execute(order(1), market()).isFilled());
                ASSERT_TRUE(target->execute(order(2, OrderSide::Sell), market()).isFilled());
            }
            const double expected_loss = (bps == 0 ? 0 : 4) + 2 * commission;
            EXPECT_EQ(portfolio.cashBalance(), money(1000 - expected_loss));
            EXPECT_EQ(portfolio.positions()[0].realized_pnl(), money(-expected_loss));
            EXPECT_EQ(portfolio.snapshot(), duplicate.snapshot());
        }
    }
    auto legacy = funded();
    auto configured = funded();
    FakeBroker old{legacy, money(2)};
    FakeBroker current{configured, TradingCosts{money(2), 0}};
    ASSERT_TRUE(old.execute(order(1), market()).isFilled());
    ASSERT_TRUE(current.execute(order(1), market()).isFilled());
    EXPECT_EQ(legacy.snapshot(), configured.snapshot());
}

TEST(FakeBroker, SlippedPriceAndCommissionBothCountForAffordability) {
    for (double cash : {200.0, 202.0, 203.0}) {
        auto portfolio = funded(cash);
        FakeBroker broker{portfolio, TradingCosts{money(2), 100}};
        expectRejected(broker, portfolio, order(1), market(), ExecutionRejection::InsufficientCash);
    }
    auto portfolio = funded(204);
    FakeBroker broker{portfolio, TradingCosts{money(2), 100}};
    ASSERT_TRUE(broker.execute(order(1), market()).isFilled());
    EXPECT_EQ(portfolio.cashBalance(), money(0));
    EXPECT_EQ(portfolio.positions()[0].average_cost(), price(102));
    expectRejected(broker, portfolio, order(2, OrderSide::Sell, 3), market(),
                   ExecutionRejection::InsufficientShares);
    expectRejected(broker, portfolio, order(1), market(), ExecutionRejection::DuplicateOrder);
    EXPECT_TRUE(broker.execute(order(2, OrderSide::Sell), market()).isFilled());
}

TEST(FakeBroker, AdverseSellSlippageCanMakeSaleCommissionUnaffordable) {
    auto portfolio = funded(100);
    FakeBroker buy{portfolio, money(0)};
    ASSERT_TRUE(buy.execute(order(1, OrderSide::Buy, 1), market()).isFilled());
    FakeBroker sell{portfolio, TradingCosts{money(100), 100}};
    expectRejected(sell, portfolio, order(2, OrderSide::Sell, 1), market(),
                   ExecutionRejection::InsufficientCash);
}

TEST(FakeBroker, RejectsSlippageOverflowUnderflowAndLostAdjustments) {
    auto portfolio = funded(std::numeric_limits<double>::max());
    FakeBroker broker{portfolio, TradingCosts{money(0), 100}};
    expectRejected(broker, portfolio, order(1, OrderSide::Buy, 1),
                   market(std::numeric_limits<double>::max()),
                   ExecutionRejection::InvalidArithmetic);
    expectRejected(broker, portfolio, order(1, OrderSide::Buy, 1),
                   market(std::numeric_limits<double>::denorm_min()),
                   ExecutionRejection::InvalidArithmetic);
    for (double bps : {std::numeric_limits<double>::denorm_min(), 1e-15}) {
        auto ordinary = funded();
        FakeBroker tiny{ordinary, TradingCosts{money(0), bps}};
        expectRejected(tiny, ordinary, order(1), market(), ExecutionRejection::InvalidArithmetic);
        FakeBroker buy{ordinary, money(0)};
        ASSERT_TRUE(buy.execute(order(1), market()).isFilled());
        expectRejected(tiny, ordinary, order(2, OrderSide::Sell), market(),
                       ExecutionRejection::InvalidArithmetic);
    }
    auto ordinary = funded();
    FakeBroker buy{ordinary, money(0)};
    ASSERT_TRUE(buy.execute(order(1), market()).isFilled());
    FakeBroker near_total{ordinary, TradingCosts{money(0), 9999}};
    expectRejected(near_total, ordinary, order(2, OrderSide::Sell),
                   market(std::numeric_limits<double>::denorm_min()),
                   ExecutionRejection::InvalidArithmetic);
}

}  // namespace
