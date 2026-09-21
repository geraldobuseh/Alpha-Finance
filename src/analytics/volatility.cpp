#include "analytics/volatility.hpp"

#include <algorithm>
#include <cmath>

namespace pql {

std::optional<Volatility> sampleVolatility(const std::vector<double>& fractional_returns,
                                           std::optional<std::uint32_t> periods_per_year) {
    if (periods_per_year && *periods_per_year == 0) {
        throw VolatilityError("Annualization requires positive periods per year");
    }
    for (double value : fractional_returns) {
        if (!std::isfinite(value) || value < -1.0) {
            throw VolatilityError("Expected finite fractional simple returns at least -1");
        }
    }
    const auto count = fractional_returns.size();
    if (count < 2) return std::nullopt;

    const auto [lowest, highest] =
        std::minmax_element(fractional_returns.begin(), fractional_returns.end());
    const double range = *highest - *lowest;
    double deviation = 0.0;
    if (range > 0.0) {
        if (!std::isfinite(range)) throw VolatilityError("Unrepresentable return range");
        // Subtract before scaling to retain nearby large observations. Normalized
        // offsets lie in [0,1], avoiding overflow/underflow from squaring raw returns.
        double mean = 0.0;
        for (double value : fractional_returns) mean += (value - *lowest) / range;
        mean /= static_cast<double>(count);
        double squared = 0.0;
        for (double value : fractional_returns) {
            const double difference = (value - *lowest) / range - mean;
            squared += difference * difference;
        }
        deviation = range * std::sqrt(squared / static_cast<double>(count - 1));
        if (!std::isfinite(deviation) || deviation <= 0.0) {
            throw VolatilityError("Unrepresentable sample standard deviation");
        }
    }
    Volatility result{deviation, std::nullopt, count};
    if (periods_per_year) {
        const double annual = deviation * std::sqrt(static_cast<double>(*periods_per_year));
        if (!std::isfinite(annual)) throw VolatilityError("Unrepresentable annualized volatility");
        result.annualized = annual;
    }
    return result;
}

}  // namespace pql
