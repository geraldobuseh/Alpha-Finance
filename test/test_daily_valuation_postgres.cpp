#include <gtest/gtest.h>

#include <future>
#include <pqxx/pqxx>

#include "persistence/postgres.hpp"

namespace {
using namespace pql;
using namespace pql::persistence;
Money money(double x) { return Money::create(x).value(); }
Price price(double x) { return Price::create(x).value(); }
Symbol symbol(const char* x) { return Symbol::create(x).value(); }
Date day(int d) { return Date::create(2026, 9, unsigned(d)).value(); }
Timestamp closeAt(int d) {
    return Timestamp{std::chrono::sys_days{day(d).value()} + std::chrono::hours{20}};
}
PriceBar bar(const Symbol& asset, int d, double x) {
    return PriceBar::create(asset, day(d), price(x), price(x), price(x), price(x),
                            Quantity::create(0).value())
        .value();
}
PortfolioId seed(const Symbol& asset, double cash = 1000) {
    PostgresUnitOfWork work;
    work.addAsset(asset, "Synthetic daily valuation asset");
    const auto id = work.createPortfolio("Daily test", PortfolioKind::Simulated, money(cash));
    work.commit();
    return id;
}
void append(PostgresUnitOfWork& work, PortfolioId id, const Symbol& asset, std::int64_t order_id,
            int d, double cost = 100, double fee = 1) {
    const auto request =
        Order::create_market(OrderId::create(order_id).value(), asset, OrderSide::Buy,
                             Quantity::create(2).value(), closeAt(d))
            .value();
    const auto trade = Trade::create(request, price(cost), closeAt(d)).value();
    work.append(request, Transaction::create(id, trade, money(fee)).value());
}
TEST(DailyPostgres, ReconnectReturnsMarksAndHistoricalCutoff) {
    const auto asset = symbol("VALHISTORY");
    const auto id = seed(asset);
    {
        PostgresUnitOfWork work;
        append(work, id, asset, 1, 14);
        append(work, id, asset, 2, 16);  // Future relative to first requested valuation.
        const auto first = work.valueDaily(id, day(14), closeAt(14), "synthetic",
                                           {bar(asset, 14, 110)}, std::nullopt);
        EXPECT_EQ(first.cash(), money(799));
        EXPECT_EQ(first.positionValue(), money(220));
        EXPECT_EQ(first.totalValue(), money(1019));
        EXPECT_EQ(first.ledgerSequence(), 1U);
        EXPECT_FALSE(first.dailyReturn());
        EXPECT_DOUBLE_EQ(*first.cumulativeReturn(), 0.019);
        work.commit();
    }
    {
        PostgresUnitOfWork work;
        const auto first = work.dailyValuation(id, day(14));
        ASSERT_TRUE(first);
        EXPECT_EQ(first->totalValue(), money(1019));
        ASSERT_EQ(first->marks().size(), 1U);
        EXPECT_EQ(first->marks()[0].price, price(110));
        const auto second =
            work.valueDaily(id, day(15), closeAt(15), "synthetic", {bar(asset, 15, 100)}, day(14));
        EXPECT_DOUBLE_EQ(*second.dailyReturn(), -20.0 / 1019.0);
        EXPECT_DOUBLE_EQ(*second.cumulativeReturn(), -0.001);
        work.commit();
    }
    PostgresUnitOfWork read;
    EXPECT_EQ(read.dailyValuation(id, day(15))->totalValue(), money(999));
    EXPECT_FALSE(read.dailyValuation(id, day(17)));
}
TEST(DailyPostgres, IdempotenceConflictRollbackAndLateTrade) {
    const auto asset = symbol("VALRETRY");
    const auto id = seed(asset);
    {
        PostgresUnitOfWork work;
        append(work, id, asset, 1, 14);
        (void)work.valueDaily(id, day(14), closeAt(14), "synthetic", {bar(asset, 14, 100)},
                              std::nullopt);
        work.commit();
    }
    {
        PostgresUnitOfWork work;
        (void)work.valueDaily(id, day(14), closeAt(14), "synthetic", {bar(asset, 14, 100)},
                              std::nullopt);
        work.commit();
    }
    {
        PostgresUnitOfWork work;
        work.renamePortfolio(id, "must roll back");
        EXPECT_THROW((void)work.valueDaily(id, day(14), closeAt(14), "synthetic",
                                           {bar(asset, 14, 101)}, std::nullopt),
                     PersistenceError);
        EXPECT_THROW(work.commit(), PersistenceError);
    }
    {
        PostgresUnitOfWork work;
        EXPECT_THROW(append(work, id, asset, 2, 14), PersistenceError);
        EXPECT_THROW(work.commit(), PersistenceError);
    }
    {
        PostgresUnitOfWork work;
        EXPECT_EQ(work.portfolio(id)->name, "Daily test");
        (void)work.valueDaily(id, day(15), closeAt(15), "synthetic", {bar(asset, 15, 100)},
                              day(14));
        // No commit: parent snapshot and its marks must both roll back.
    }
    PostgresUnitOfWork read;
    EXPECT_FALSE(read.dailyValuation(id, day(15)));
    EXPECT_EQ(read.transactions(id).size(), 1U);
}
TEST(DailyPostgres, MissingPricesPoisonPriorWritesAndGapRejects) {
    const auto asset = symbol("VALMISSING");
    const auto id = seed(asset);
    {
        PostgresUnitOfWork work;
        append(work, id, asset, 1, 14);
        EXPECT_THROW((void)work.valueDaily(id, day(14), closeAt(14), "synthetic", {}, std::nullopt),
                     ValuationError);
        EXPECT_THROW(work.commit(), PersistenceError);
    }
    {
        PostgresUnitOfWork work;
        EXPECT_TRUE(work.transactions(id).empty());
        EXPECT_FALSE(work.dailyValuation(id, day(14)));
        (void)work.valueDaily(id, day(14), closeAt(14), "synthetic", {}, std::nullopt);
        work.commit();
    }
    {
        PostgresUnitOfWork work;
        EXPECT_THROW((void)work.valueDaily(id, day(16), closeAt(16), "synthetic", {}, day(15)),
                     PersistenceError);
    }
    {
        PostgresUnitOfWork work;
        EXPECT_THROW((void)work.valueDaily(id, day(13), closeAt(13), "synthetic", {}, std::nullopt),
                     PersistenceError);
    }
}
TEST(DailyPostgres, DecimalGeneratedSumAndZeroCapitalRoundTrip) {
    const auto asset = symbol("VALDECIMAL");
    const auto id = seed(asset, 0.3);
    double engine_total = 0;
    {
        PostgresUnitOfWork work;
        append(work, id, asset, 1, 14, 0.1, 0);
        const auto value = work.valueDaily(id, day(14), closeAt(14), "synthetic",
                                           {bar(asset, 14, 0.15)}, std::nullopt);
        engine_total = value.totalValue().value();
        work.commit();
    }
    {
        PostgresUnitOfWork work;
        EXPECT_DOUBLE_EQ(work.dailyValuation(id, day(14))->totalValue().value(), engine_total);
    }
    const auto empty_id = seed(symbol("VALZERO"), 0);
    {
        PostgresUnitOfWork work;
        const auto first =
            work.valueDaily(empty_id, day(14), closeAt(14), "synthetic", {}, std::nullopt);
        EXPECT_FALSE(first.dailyReturn());
        EXPECT_FALSE(first.cumulativeReturn());
        const auto next = work.valueDaily(empty_id, day(15), closeAt(15), "synthetic", {}, day(14));
        EXPECT_FALSE(next.dailyReturn());
        work.commit();
    }
    PostgresUnitOfWork read;
    EXPECT_FALSE(read.dailyValuation(empty_id, day(15))->cumulativeReturn());
}
TEST(DailyPostgres, CompetingIdenticalValuationsSerialize) {
    const auto id = seed(symbol("VALCONCURRENT"));
    PostgresUnitOfWork first;
    (void)first.valueDaily(id, day(14), closeAt(14), "synthetic", {}, std::nullopt);
    std::promise<void> ready;
    auto signal = ready.get_future();
    auto competing = std::async(std::launch::async, [&] {
        PostgresUnitOfWork work;
        ready.set_value();
        const auto result =
            work.valueDaily(id, day(14), closeAt(14), "synthetic", {}, std::nullopt);
        work.commit();
        return result.totalValue();
    });
    ASSERT_EQ(signal.wait_for(std::chrono::seconds(5)), std::future_status::ready);
    EXPECT_EQ(competing.wait_for(std::chrono::milliseconds(100)), std::future_status::timeout);
    first.commit();
    EXPECT_EQ(competing.get(), money(1000));
}
}  // namespace
