#pragma once

#include <cmath>
#include <stdexcept>

#include "domain/order.hpp"

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

    // Shared quote-to-fill estimate for proposal sizing and authoritative execution.
    [[nodiscard]] std::optional<Price> executionPrice(Price reference,
                                                      OrderSide side) const noexcept {
        if (side != OrderSide::Buy && side != OrderSide::Sell) return std::nullopt;
        if (slippage_basis_points_ == 0.0) return reference;
        const double quote = reference.value();
        const double delta = quote * (slippage_basis_points_ / 10000.0);
        const double adjusted = side == OrderSide::Buy ? quote + delta : quote - delta;
        const auto result = Price::create(adjusted);
        if (delta <= 0.0 || !result ||
            (side == OrderSide::Buy ? adjusted <= quote : adjusted >= quote))
            return std::nullopt;
        return result;
    }

   private:
    Money commission_;
    double slippage_basis_points_;
};

}  // namespace pql
