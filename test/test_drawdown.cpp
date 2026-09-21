#include <gtest/gtest.h>

#include <cmath>
#include <limits>

#include "analytics/drawdown.hpp"

namespace {
using namespace pql;
EquityPoint point(double value, int tick) {
    return {Timestamp{Timestamp::Value{std::chrono::milliseconds{tick}}},
            Money::create(value).value()};
}
std::vector<EquityPoint> curve(std::initializer_list<double> values) {
    std::vector<EquityPoint> result;
    int tick = 0;
    for (double value : values) result.push_back(point(value, tick++));
    return result;
}

TEST(Drawdown, TicketExampleReportsPeakTroughAndNegativeTwentyFivePercent) {
    const auto data = curve({1000, 1200, 1100, 900, 1300});
    const auto result = maximumDrawdown(data);
    ASSERT_TRUE(result);
    EXPECT_EQ(result->peak, data[1]);
    EXPECT_EQ(result->trough, data[3]);
    EXPECT_EQ(result->fraction, -0.25);
}

TEST(Drawdown, MeasuresPercentageNotDollarLossAndRequiresEarlierPeak) {
    const auto data = curve({100, 50, 1000, 700, 1200});
    const auto result = maximumDrawdown(data);
    ASSERT_TRUE(result);
    EXPECT_EQ(result->fraction, -0.5);  // $50 loss is worse proportionally than $300.
    EXPECT_EQ(result->peak, data[0]);
    EXPECT_EQ(result->trough, data[1]);
    // The last/global maximum must not be paired with the earlier/global minimum.
    EXPECT_EQ(maximumDrawdown(curve({900, 1200}))->fraction, 0);
}

TEST(Drawdown, NewHighCanBeginDeeperDeclineAndRecoveryDoesNotEraseIt) {
    const auto data = curve({100, 90, 200, 100, 300});
    const auto result = maximumDrawdown(data);
    ASSERT_TRUE(result);
    EXPECT_EQ(result->fraction, -0.5);
    EXPECT_EQ(result->peak, data[2]);
    EXPECT_EQ(result->trough, data[3]);
}

TEST(Drawdown, TiesRetainEarliestPeakAndEarliestEqualTrough) {
    const auto data = curve({100, 100, 75, 75, 200, 150});
    const auto result = maximumDrawdown(data);
    ASSERT_TRUE(result);
    EXPECT_EQ(result->fraction, -0.25);
    EXPECT_EQ(result->peak, data[0]);
    EXPECT_EQ(result->trough, data[2]);
}

TEST(Drawdown, NoPositiveBaselineIsAbsentAndNoDeclineUsesFirstPositivePoint) {
    EXPECT_FALSE(maximumDrawdown({}));
    EXPECT_FALSE(maximumDrawdown(curve({0, 0, 0})));
    for (const auto& data : {curve({100}), curve({100, 100, 100}), curve({100, 110, 120})}) {
        const auto result = maximumDrawdown(data);
        ASSERT_TRUE(result);
        EXPECT_EQ(result->fraction, 0);
        EXPECT_EQ(result->peak, data.front());
        EXPECT_EQ(result->trough, data.front());
    }
    const auto leading = curve({0, 0, 100, 90});
    EXPECT_EQ(maximumDrawdown(leading)->peak, leading[2]);
    EXPECT_EQ(maximumDrawdown(leading)->fraction, -0.1);
}

TEST(Drawdown, TotalLossAndMalformedSuffixStillValidate) {
    const auto data = curve({100, 0, 0});
    const auto result = maximumDrawdown(data);
    ASSERT_TRUE(result);
    EXPECT_EQ(result->fraction, -1);
    EXPECT_EQ(result->trough, data[1]);
    EXPECT_THROW((void)maximumDrawdown(curve({100, 0, -1})), DrawdownError);
    EXPECT_THROW((void)maximumDrawdown({point(100, 0), point(0, 1), point(0, 1)}), DrawdownError);
}

TEST(Drawdown, NegativeAndUnorderedObservationsRejectRatherThanSort) {
    EXPECT_THROW((void)maximumDrawdown(curve({-1})), DrawdownError);
    EXPECT_THROW((void)maximumDrawdown({point(100, 1), point(90, 0)}), DrawdownError);
    EXPECT_THROW((void)maximumDrawdown({point(100, 1), point(90, 1)}), DrawdownError);
    EXPECT_THROW((void)maximumDrawdown({point(0, 1), point(0, 1)}), DrawdownError);
}

TEST(Drawdown, TinyDeclinesPreservedAndFalseTotalLossRejects) {
    const double lower = std::nextafter(1.0, 0.0);
    const auto tiny = maximumDrawdown(curve({1, lower}));
    ASSERT_TRUE(tiny);
    EXPECT_EQ(tiny->fraction, lower - 1);
    const double huge = std::numeric_limits<double>::max();
    EXPECT_EQ(maximumDrawdown(curve({huge, huge / 2}))->fraction, -0.5);
    EXPECT_THROW((void)maximumDrawdown(curve({huge, std::numeric_limits<double>::min()})),
                 DrawdownError);
}

TEST(Drawdown, IdenticalInputsYieldExactlyEqualOwnedOutputs) {
    auto data = curve({1000, 1200, 900, 1300});
    const auto before = data;
    const auto first = maximumDrawdown(data);
    EXPECT_EQ(first, maximumDrawdown(data));
    EXPECT_EQ(data, before);
    data.clear();
    ASSERT_TRUE(first);
    EXPECT_EQ(first->peak, before[1]);
    EXPECT_EQ(first->trough, before[2]);
}
}  // namespace
