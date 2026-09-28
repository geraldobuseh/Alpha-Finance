#include <gtest/gtest.h>

#include <cmath>
#include <limits>

#include "analytics/sharpe.hpp"

namespace {
using namespace pql;
TEST(Sharpe, KnownPositiveNegativeAndZeroMeanWithSampleDispersion) {
    const auto positive = sampleSharpe({0, 0.02});
    ASSERT_TRUE(positive);
    EXPECT_NEAR(positive->per_period, 1 / std::sqrt(2.0), 1e-15);
    EXPECT_EQ(positive->observations, 2U);
    EXPECT_FALSE(positive->annualized);
    EXPECT_NEAR(sampleSharpe({-0.02, 0})->per_period, -1 / std::sqrt(2.0), 1e-15);
    EXPECT_EQ(sampleSharpe({-0.01, 0, 0.01})->per_period, 0);
    EXPECT_NEAR(sampleSharpe({0.01, 0.02, 0.03})->per_period, 2, 1e-15);
}
TEST(Sharpe, AnnualizesOnlyWithExplicitSamplingConvention) {
    const auto result = sampleSharpe({0, 0.02}, 252);
    ASSERT_TRUE(result);
    ASSERT_TRUE(result->annualized);
    EXPECT_NEAR(*result->annualized, std::sqrt(126.0), 1e-14);
    EXPECT_NEAR(*sampleSharpe({0, 0.02}, 12)->annualized, std::sqrt(6.0), 1e-15);
    EXPECT_EQ(*sampleSharpe({0, 0.02}, 1)->annualized, result->per_period);
    EXPECT_LT(*sampleSharpe({-0.02, 0}, 252)->annualized, 0);
}
TEST(Sharpe, ZeroDispersionAndInsufficientSamplesRemainUndefined) {
    EXPECT_FALSE(sampleSharpe({}));
    EXPECT_FALSE(sampleSharpe({0.01}));
    for (double value : {-1.0, -0.01, 0.0, 0.01, std::numeric_limits<double>::max()})
        EXPECT_FALSE(sampleSharpe({value, value, value}, 252));
}
TEST(Sharpe, InvalidInputsRejectEvenBeforeUndefinedShortcut) {
    for (double value :
         {-1.1, std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity(),
          -std::numeric_limits<double>::infinity()}) {
        EXPECT_THROW((void)sampleSharpe({value}), SharpeError);
        EXPECT_THROW((void)sampleSharpe({0, 0, value}), SharpeError);
    }
    EXPECT_THROW((void)sampleSharpe({}, 0), SharpeError);
    EXPECT_THROW((void)sampleSharpe({0, 0}, 0), SharpeError);
}
TEST(Sharpe, CompensatedMeanRetainsSmallResidualAfterCancellation) {
    const auto result = sampleSharpe({1, 1e-16, -1});
    ASSERT_TRUE(result);
    EXPECT_GT(result->per_period, 0);
    EXPECT_NEAR(result->per_period, 1e-16 / 3, 1e-31);
    EXPECT_NEAR(sampleSharpe({-1, -1e-16, 1})->per_period, -1e-16 / 3, 1e-31);
}
TEST(Sharpe, AvoidsOverflowingRawSumOrAnnualVolatility) {
    const double large = std::numeric_limits<double>::max();
    const auto result = sampleSharpe({large / 2, large}, 252);
    ASSERT_TRUE(result);
    EXPECT_NEAR(result->per_period, 3 / std::sqrt(2.0), 1e-14);
    EXPECT_TRUE(std::isfinite(*result->annualized));
    const double adjacent = std::nextafter(1e200, std::numeric_limits<double>::infinity());
    EXPECT_TRUE(std::isfinite(sampleSharpe({1e200, adjacent})->per_period));
}
TEST(Sharpe, TinyValidReturnsAndUnrepresentableMeanAreExplicit) {
    EXPECT_NEAR(sampleSharpe({0, 1e-200, 2e-200})->per_period, 1, 1e-15);
    const double minimum = std::numeric_limits<double>::denorm_min();
    EXPECT_THROW((void)sampleSharpe({0, minimum}), SharpeError);
    EXPECT_THROW((void)sampleSharpe({0, 0, 0, 0, minimum}), SharpeError);
}
TEST(Sharpe, RepeatedCallsPreserveInputsAndExactResults) {
    const std::vector<double> returns{-0.1, 0.2, 0.03};
    const auto before = returns;
    EXPECT_EQ(sampleSharpe(returns, 252), sampleSharpe(returns, 252));
    EXPECT_EQ(returns, before);
}
TEST(Sharpe, SubnormalIntermediatesRejectRatherThanDistortTheRatio) {
    const double tiny = std::numeric_limits<double>::denorm_min();
    // Rounded mean/deviation previously produced 2 instead of 3/sqrt(2).
    EXPECT_THROW((void)sampleSharpe({tiny, 2 * tiny}), SharpeError);
    EXPECT_THROW((void)sampleSharpe({1, tiny, -1}), SharpeError);
    EXPECT_THROW((void)sampleSharpe({1, std::numeric_limits<double>::min(), -1}), SharpeError);
}
TEST(Sharpe, PositiveScalingPreservesRatioWithinNormalPrecision) {
    EXPECT_NEAR(sampleSharpe({0, 0.02})->per_period, sampleSharpe({0, 0.2})->per_period, 1e-15);
}
}  // namespace
