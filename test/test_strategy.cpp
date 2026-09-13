#include "strategy/strategy.hpp"

// Keep the interface first to verify self-contained public includes.
#include <gtest/gtest.h>

#include <memory>
#include <type_traits>

#include "execution/fake_broker.hpp"

namespace {
using namespace pql;

static_assert(std::is_abstract_v<Strategy>);
static_assert(std::has_virtual_destructor_v<Strategy>);
static_assert(
    std::is_same_v<decltype(&Strategy::generateOrders),
                   std::vector<Order> (Strategy::*)(const MarketState&, const PortfolioState&)>);
static_assert(std::is_same_v<decltype(&Strategy::name), std::string (Strategy::*)() const>);
static_assert(!std::is_convertible_v<Portfolio&, const PortfolioState&>);
template <typename T>
concept CanApplyTrade = requires(T& value, const Trade& trade) { value.applyTrade(trade); };
template <typename T>
concept CanApplyTransaction =
    requires(T& value, const Transaction& event) { value.applyTransaction(event); };
static_assert(!CanApplyTrade<PortfolioState> && !CanApplyTransaction<PortfolioState>);
static_assert(std::is_same_v<decltype(std::declval<PortfolioState&>().positions()),
                             const std::vector<Position>&>);
static_assert(std::is_same_v<decltype(std::declval<PortfolioState&>().transactionHistory()),
                             const std::vector<Transaction>&>);

Money money(double value) { return Money::create(value).value(); }
Portfolio funded(double cash) {
    return Portfolio::create(PortfolioId::create(1).value(), money(cash)).value();
}
MarketState quote() {
    return MarketState{Symbol::create("AAPL").value(), Price::create(100).value(),
                       Timestamp{Timestamp::Value{std::chrono::milliseconds{10}}}};
}

// Test policy only. IDs are supplied by the fixture, not a production allocator.
// Observes affordability at the quote and proposes one share when no positions
// exist. Broker remains responsible for actual affordability including costs.
class ProposeOne final : public Strategy {
   public:
    ProposeOne(OrderId id, bool& destroyed) : id_(id), destroyed_(destroyed) {}
    ~ProposeOne() override { destroyed_ = true; }
    std::string name() const override { return "synthetic-propose-one"; }
    std::vector<Order> generateOrders(const MarketState& market,
                                      const PortfolioState& portfolio) override {
        if (!portfolio.positions().empty() ||
            portfolio.cashBalance().value() < market.price().value())
            return {};
        return {Order::create_market(id_, market.symbol(), OrderSide::Buy,
                                     Quantity::create(1).value(), market.timestamp())
                    .value()};
    }

   private:
    OrderId id_;
    bool& destroyed_;
};

TEST(Strategy, ProposalsUseObservedStateWithoutExecutingAndVirtualDestructionWorks) {
    auto portfolio = funded(1000);
    const PortfolioState state = portfolio.snapshot();
    const auto market = quote();
    bool destroyed = false;
    std::vector<Order> proposals;
    {
        std::unique_ptr<Strategy> strategy =
            std::make_unique<ProposeOne>(OrderId::create(1).value(), destroyed);
        const Strategy& view = *strategy;
        EXPECT_EQ(view.name(), "synthetic-propose-one");
        proposals = strategy->generateOrders(market, state);
        EXPECT_EQ(proposals, strategy->generateOrders(market, state));
        EXPECT_EQ(portfolio.snapshot(), state);
    }
    EXPECT_TRUE(destroyed);
    ASSERT_EQ(proposals.size(), 1U);
    EXPECT_EQ(proposals[0].symbol(), market.symbol());
    EXPECT_EQ(proposals[0].quantity(), Quantity::create(1).value());
    EXPECT_EQ(proposals[0].side(), OrderSide::Buy);
    EXPECT_EQ(proposals[0].timestamp(), market.timestamp());
    EXPECT_EQ(proposals[0].status(), OrderStatus::Pending);
    // Only orchestration holds the live portfolio and invokes execution.
    FakeBroker broker{portfolio, money(1)};
    ASSERT_TRUE(broker.execute(proposals[0], market).isFilled());
    EXPECT_EQ(portfolio.cashBalance(), money(899));
    ASSERT_EQ(portfolio.transactionHistory().size(), 1U);
    EXPECT_EQ(portfolio.transactionHistory()[0].order_id(), proposals[0].id());
    EXPECT_EQ(state.cashBalance(), money(1000));
    EXPECT_TRUE(state.positions().empty());
    EXPECT_TRUE(state.transactionHistory().empty());
    EXPECT_EQ(market.price(), Price::create(100).value());
}

TEST(Strategy, NoActionAndFreshSnapshotDecisionsRemainDetached) {
    auto portfolio = funded(1000);
    auto empty_cash = funded(0);
    const PortfolioState old_state = portfolio.snapshot();
    const auto market = quote();
    bool destroyed = false;
    ProposeOne fixture{OrderId::create(1).value(), destroyed};
    Strategy& strategy = fixture;
    EXPECT_TRUE(strategy.generateOrders(market, empty_cash.snapshot()).empty());
    const auto proposals = strategy.generateOrders(market, old_state);
    ASSERT_EQ(proposals.size(), 1U);
    FakeBroker broker{portfolio, money(0)};
    ASSERT_TRUE(broker.execute(proposals[0], market).isFilled());
    const auto after_execution = portfolio.snapshot();
    EXPECT_TRUE(strategy.generateOrders(market, after_execution).empty());
    EXPECT_EQ(strategy.generateOrders(market, old_state), proposals);
    EXPECT_EQ(portfolio.snapshot(), after_execution);
}

TEST(Strategy, BrokerMayRejectProposalsWithoutPortfolioConsequences) {
    auto portfolio = funded(100);
    const PortfolioState state = portfolio.snapshot();
    bool destroyed = false;
    ProposeOne fixture{OrderId::create(1).value(), destroyed};
    Strategy& strategy = fixture;
    const auto proposals = strategy.generateOrders(quote(), state);
    ASSERT_EQ(proposals.size(), 1U);
    FakeBroker broker{portfolio, money(1)};
    const auto result = broker.execute(proposals[0], quote());
    EXPECT_FALSE(result.isFilled());
    EXPECT_EQ(result.rejectionReason(), ExecutionRejection::InsufficientCash);
    EXPECT_EQ(portfolio.snapshot(), state);
}

}  // namespace
