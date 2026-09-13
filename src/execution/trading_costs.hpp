#pragma once

#include <cmath>
#include <stdexcept>

#include "domain/financial_types.hpp"

namespace pql {

// Single-currency flat commission per full fill and adverse slippage in basis
// points (1bp = 0.01%). Explicit zero costs are valid simulation controls.
class TradingCosts {
   public:
    TradingCosts(Money commission, double slippage_basis_points)
        : commission_(commission), slippage_basis_points_(slippage_basis_points) {
        if (commission.value() < 0.0 || !std::isfinite(slippage_basis_points) ||
            slippage_basis_points < 0.0 || slippage_basis_points >= 10000.0) {
            throw std::invalid_argument(
                "Costs require nonnegative commission and slippage in [0,10000) bps");
        }
    }
    [[nodiscard]] Money commission() const noexcept { return commission_; }
    [[nodiscard]] double slippageBasisPoints() const noexcept { return slippage_basis_points_; }

   private:
    Money commission_;
    double slippage_basis_points_;
};

}  // namespace pql
