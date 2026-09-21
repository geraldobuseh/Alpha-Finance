#include "analytics/drawdown.hpp"

#include "analytics/returns.hpp"

namespace pql {

std::optional<MaximumDrawdown> maximumDrawdown(const std::vector<EquityPoint>& curve) {
    std::optional<Timestamp> previous_time;
    std::optional<EquityPoint> peak;
    std::optional<MaximumDrawdown> worst;
    for (const auto& point : curve) {
        if (point.value.value() < 0.0 || (previous_time && point.timestamp <= *previous_time)) {
            throw DrawdownError("Negative equity or non-increasing observation timestamps");
        }
        previous_time = point.timestamp;
        if (!peak) {
            if (point.value.value() == 0.0) continue;
            peak = point;
            worst = MaximumDrawdown{0.0, point, point};
        } else if (point.value.value() > peak->value.value()) {
            peak = point;
        } else {
            try {
                const double decline = *cumulativeReturn(point.value, peak->value);
                if (decline < worst->fraction) worst = MaximumDrawdown{decline, *peak, point};
            } catch (const ReturnError& error) {
                throw DrawdownError(error.what());
            }
        }
    }
    return worst;
}

}  // namespace pql
