#include <gtest/gtest.h>

#include <cmath>
#include <limits>

#include "analytics/volatility.hpp"

namespace {
using namespace pql;
TEST(Volatility, SampleDeviationUsesNMinusOneAndFractionalUnits) {
    const auto result = sampleVolatility({-0.01, 0, 0.01});
    ASSERT_TRUE(result);
    EXPECT_NEAR(result->per_period, 0.01, 1e-17);
    EXPECT_EQ(result->observations, 3U);
    EXPECT_FALSE(result->annualized);
    EXPECT_NEAR(sampleVolatility({0, 0.02})->per_period, 0.02 / std::sqrt(2.0), 1e-17);
}
TEST(Volatility, ExplicitAnnualizationUsesSquareRootOfSamplingFrequency) {
    const auto daily = sampleVolatility({-0.01, 0, 0.01}, 252);
    ASSERT_TRUE(daily);
    ASSERT_TRUE(daily->annualized);
    EXPECT_NEAR(*daily->annualized, 0.01 * std::sqrt(252.0), 1e-15);
    EXPECT_NEAR(*sampleVolatility({-0.01, 0, 0.01}, 12)->annualized, 0.01 * std::sqrt(12.0), 1e-16);
    EXPECT_EQ(*sampleVolatility({-0.01, 0, 0.01}, 1)->annualized, daily->per_period);
}
TEST(Volatility, InsufficientSamplesAreAbsentAndConstantReturnsHaveZeroDispersion) {
    EXPECT_FALSE(sampleVolatility({}));
    EXPECT_FALSE(sampleVolatility({0.1}, 252));
    for (double value : {-1.0, 0.0, 0.1, std::numeric_limits<double>::max()}) {
        const auto result = sampleVolatility({value, value, value}, 252);
        ASSERT_TRUE(result);
        EXPECT_EQ(result->per_period, 0);
        EXPECT_EQ(*result->annualized, 0);
    }
}
TEST(Volatility, InvalidInputNeverBecomesMissingOrZeroVolatility) {
    for (double value :
         {-1.01, std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity(),
          std::numeric_limits<double>::quiet_NaN()}) {
        EXPECT_THROW((void)sampleVolatility({value}), VolatilityError);
        EXPECT_THROW((void)sampleVolatility({0, 0, value}), VolatilityError);
    }
    EXPECT_THROW((void)sampleVolatility({}, 0), VolatilityError);
    EXPECT_THROW((void)sampleVolatility({0, 0}, 0), VolatilityError);
    EXPECT_GT(sampleVolatility({-1, 0})->per_period, 0);
}
TEST(Volatility, LevelAndDirectionDoNotDefineDispersion) {
    const auto positive = sampleVolatility({0.01, 0.02, 0.03});
    const auto negative = sampleVolatility({-0.01, -0.02, -0.03});
    ASSERT_TRUE(positive);
    ASSERT_TRUE(negative);
    EXPECT_NEAR(positive->per_period, 0.01, 1e-17);
    EXPECT_EQ(positive->per_period, negative->per_period);
}
TEST(Volatility, CenteringRetainsNearbyLargeValuesWithoutSquaringOverflow) {
    const double high = 1e200;
    const double next = std::nextafter(high, std::numeric_limits<double>::infinity());
    const double expected = (next - high) / std::sqrt(2.0);
    const auto result = sampleVolatility({high, next});
    ASSERT_TRUE(result);
    EXPECT_NEAR(result->per_period / expected, 1, 1e-15);
    const double maximum = std::numeric_limits<double>::max();
    EXPECT_NEAR(sampleVolatility({0, maximum})->per_period / maximum, 1 / std::sqrt(2.0), 1e-15);
    EXPECT_THROW((void)sampleVolatility({0, maximum}, 252), VolatilityError);
}
TEST(Volatility, ScalingRetainsTinyReturnsAndRejectsUnrepresentableZero) {
    const auto tiny = sampleVolatility({0, 1e-200, 2e-200});
    ASSERT_TRUE(tiny);
    EXPECT_NEAR(tiny->per_period / 1e-200, 1, 1e-15);
    const double minimum = std::numeric_limits<double>::denorm_min();
    EXPECT_THROW((void)sampleVolatility({0, 0, 0, 0, minimum}), VolatilityError);
}
TEST(Volatility, IdenticalInputIsDeterministicAndUnchanged) {
    const std::vector<double> returns{-0.1, 0.2, 0.05, -0.03};
    const auto before = returns;
    EXPECT_EQ(sampleVolatility(returns, 252), sampleVolatility(returns, 252));
    EXPECT_EQ(returns, before);
}
}  // namespace
