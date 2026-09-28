#include "analytics/sharpe.hpp"

#include <algorithm>
#include <cmath>

#include "analytics/volatility.hpp"

namespace pql {

std::optional<Sharpe> sampleSharpe(const std::vector<double>& fractional_returns,
                                   std::optional<std::uint32_t> periods_per_year) {
    if (periods_per_year && *periods_per_year == 0) {
        throw SharpeError("Annualization requires positive periods per year");
    }
    std::optional<Volatility> volatility;
    try {
        // Annualize the ratio, not intermediate volatility: the latter could
        // overflow even when the final dimensionless ratio is representable.
        volatility = sampleVolatility(fractional_returns);
    } catch (const VolatilityError& error) {
        throw SharpeError(error.what());
    }
    if (!volatility || volatility->per_period == 0.0) return std::nullopt;
    if (std::fpclassify(volatility->per_period) == FP_SUBNORMAL) {
        throw SharpeError("Sample deviation has insufficient precision for a Sharpe ratio");
    }

    double scale = 0.0;
    for (double value : fractional_returns) scale = std::max(scale, std::abs(value));
    // Neumaier compensated summation on normalized values avoids raw sum
    // overflow and retains small terms when opposite-sign returns cancel.
    double sum = 0.0;
    double correction = 0.0;
    for (double value : fractional_returns) {
        const double normalized = value / scale;
        if ((value != 0.0 && normalized == 0.0) || std::fpclassify(normalized) == FP_SUBNORMAL) {
            throw SharpeError("Unrepresentable scaled return");
        }
        const double next = sum + normalized;
        correction += std::abs(sum) >= std::abs(normalized) ? (sum - next) + normalized
                                                            : (normalized - next) + sum;
        sum = next;
    }
    const double total = sum + correction;
    const double mean = (total / static_cast<double>(fractional_returns.size())) * scale;
    if (!std::isfinite(mean) || (total != 0.0 && mean == 0.0) ||
        std::fpclassify(mean) == FP_SUBNORMAL) {
        throw SharpeError("Unrepresentable mean return");
    }
    const double ratio = mean / volatility->per_period;
    if (!std::isfinite(ratio) || (mean != 0.0 && ratio == 0.0) ||
        std::fpclassify(ratio) == FP_SUBNORMAL) {
        throw SharpeError("Unrepresentable Sharpe ratio");
    }
    Sharpe result{ratio, std::nullopt, fractional_returns.size()};
    if (periods_per_year) {
        const double annual = ratio * std::sqrt(static_cast<double>(*periods_per_year));
        if (!std::isfinite(annual)) throw SharpeError("Unrepresentable annualized Sharpe ratio");
        result.annualized = annual;
    }
    return result;
}

}  // namespace pql
