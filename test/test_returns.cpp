#include <gtest/gtest.h>

#include <cmath>
#include <limits>

#include "analytics/returns.hpp"

namespace {
using namespace pql;
Money money(double x) { return Money::create(x).value(); }

TEST(Returns, DerivesSimpleDailyAndCumulativeChanges) {
    EXPECT_DOUBLE_EQ(*dailyReturn(money(1100), money(1000)), 0.1);
    EXPECT_DOUBLE_EQ(*dailyReturn(money(990), money(1100)), -0.1);
    EXPECT_DOUBLE_EQ(*cumulativeReturn(money(990), money(1000)), -0.01);
    const double compounded =
        (1 + *dailyReturn(money(1100), money(1000))) * (1 + *dailyReturn(money(990), money(1100))) -
        1;
    EXPECT_NEAR(compounded, *cumulativeReturn(money(990), money(1000)), 1e-15);
    EXPECT_DOUBLE_EQ(*cumulativeReturn(money(999), money(1000)), -0.001);
}

TEST(Returns, PreservesExistingBinaryOperationOrder) {
    for (const auto& values :
         {std::pair{0.3, 0.2}, std::pair{1019.0, 999.0}, std::pair{990.0, 1000.0}}) {
        const auto [current, base] = values;
        const double expected = (current - base) / base;
        EXPECT_EQ(*dailyReturn(money(current), money(base)), expected);
        EXPECT_EQ(*cumulativeReturn(money(current), money(base)), expected);
        EXPECT_EQ(*annualizedReturn(money(current), money(base), std::chrono::days{365}), expected);
    }
}

TEST(Returns, AnnualizationIsGeometricWithExplicitCalendarDays) {
    EXPECT_NEAR(*annualizedReturn(money(121), money(100), std::chrono::days{730}), 0.1, 1e-15);
    EXPECT_NEAR(*annualizedReturn(money(81), money(100), std::chrono::days{730}), -0.1, 1e-15);
    EXPECT_NEAR(*annualizedReturn(money(200), money(100), std::chrono::days{730}),
                std::sqrt(2.0) - 1, 1e-15);
    EXPECT_LT(*annualizedReturn(money(110), money(100), std::chrono::days{366}), 0.1);
    // Half of a fixed year is not representable in integral days; use five days
    // of doubling and compare the equivalent 73 doubling periods independently.
    EXPECT_NEAR(*annualizedReturn(money(200), money(100), std::chrono::days{5}),
                std::ldexp(1.0, 73) - 1, 2e8);  // Relative tolerance below 3e-14.
}

TEST(Returns, ZeroDenominatorsFlatValuesAndTotalLoss) {
    EXPECT_FALSE(dailyReturn(money(0), money(0)));
    EXPECT_FALSE(cumulativeReturn(money(100), money(0)));
    EXPECT_FALSE(annualizedReturn(money(100), money(0), std::chrono::days{730}));
    EXPECT_EQ(*dailyReturn(money(100), money(100)), 0);
    EXPECT_EQ(*cumulativeReturn(money(0), money(100)), -1);
    for (auto elapsed : {std::chrono::days{1}, std::chrono::days{365}, std::chrono::days{730}}) {
        EXPECT_EQ(*annualizedReturn(money(100), money(100), elapsed), 0);
        EXPECT_EQ(*annualizedReturn(money(0), money(100), elapsed), -1);
    }
}

TEST(Returns, InvalidInputsFailBeforeUndefinedBaselineShortcut) {
    for (const auto& values :
         {std::pair{-1.0, 100.0}, std::pair{100.0, -1.0}, std::pair{-1.0, 0.0}}) {
        EXPECT_THROW((void)dailyReturn(money(values.first), money(values.second)), ReturnError);
        EXPECT_THROW((void)cumulativeReturn(money(values.first), money(values.second)),
                     ReturnError);
        EXPECT_THROW(
            (void)annualizedReturn(money(values.first), money(values.second), std::chrono::days{1}),
            ReturnError);
    }
    for (auto duration : {std::chrono::days{0}, std::chrono::days{-1}}) {
        EXPECT_THROW((void)annualizedReturn(money(100), money(100), duration), ReturnError);
        EXPECT_THROW((void)annualizedReturn(money(0), money(0), duration), ReturnError);
    }
}

TEST(Returns, TinyChangesAreNotLostToOnePlusRounding) {
    const double next = std::nextafter(1.0, 2.0);
    const double down = std::nextafter(1.0, 0.0);
    EXPECT_GT(*annualizedReturn(money(next), money(1), std::chrono::days{730}), 0);
    EXPECT_LT(*annualizedReturn(money(down), money(1), std::chrono::days{730}), 0);
    EXPECT_NEAR(*annualizedReturn(money(next), money(1), std::chrono::days{730}), (next - 1) / 2,
                1e-30);
}

TEST(Returns, ExtremeEndpointRatioCanHaveRepresentableAnnualizedGrowth) {
    const double big = std::numeric_limits<double>::max();
    const double small = std::numeric_limits<double>::min();
    EXPECT_THROW((void)cumulativeReturn(money(big), money(small)), ReturnError);
    const auto annual = annualizedReturn(money(big), money(small), std::chrono::days{36500});
    ASSERT_TRUE(annual);
    EXPECT_TRUE(std::isfinite(*annual));
    EXPECT_GT(*annual, 0);
    const auto loss = annualizedReturn(money(small), money(big), std::chrono::days{36500});
    ASSERT_TRUE(loss);
    EXPECT_GT(*loss, -1);
    EXPECT_LT(*loss, 0);
}

TEST(Returns, SevereLossRetainsPrecisionOfRemainingWealth) {
    EXPECT_NEAR(*annualizedReturn(money(2), money(1e16), std::chrono::days{36500}),
                -0.30335696413204849, 1e-15);
    const double expected = std::expm1((std::log(3.0) - 54 * std::log(2.0)) / 100);
    EXPECT_NEAR(*annualizedReturn(money(3), money(std::ldexp(1.0, 54)), std::chrono::days{36500}),
                expected, 1e-15);
}

TEST(Returns, UnrepresentableResultsFailRatherThanClipping) {
    const double big = std::numeric_limits<double>::max();
    const double tiny = std::numeric_limits<double>::denorm_min();
    EXPECT_THROW((void)annualizedReturn(money(big), money(1), std::chrono::days{1}), ReturnError);
    EXPECT_THROW((void)annualizedReturn(money(tiny), money(1), std::chrono::days{1}), ReturnError);
    EXPECT_THROW((void)dailyReturn(money(tiny), money(big)), ReturnError);
}
TEST(Returns, SubnormalGrossRatioUsesEndpointLogarithms) {
    const double tiny = std::numeric_limits<double>::denorm_min();
    const double expected = std::expm1((std::log(tiny) - std::log(1.5)) / 100);
    EXPECT_NEAR(*annualizedReturn(money(tiny), money(1.5), std::chrono::days{36500}), expected,
                1e-15);
}
}  // namespace
