#pragma once

#include <cmath>

#include "domain/financial_types.hpp"

namespace pql::detail {

// Cash-budget sizing shared by simple strategies. Never alters an existing order.
inline std::optional<Quantity> sizeCashBudgetBuy(Money cash_budget, Price fill_price,
                                                 Money commission) {
    const double cash = cash_budget.value();
    const double fee = commission.value();
    const double budget = cash - fee;
    if (fee < 0.0 || budget <= 0.0 || (fee > 0.0 && budget == cash)) return std::nullopt;
    double units = budget / fill_price.value();
    if (!std::isfinite(units) || units <= 0.0) return std::nullopt;
    // Round the proposal down only if division would exceed its cash budget.
    if (units * fill_price.value() + fee > cash) {
        units = std::nextafter(std::nextafter(budget, 0.0) / fill_price.value(), 0.0);
    }
    const double notional = units * fill_price.value();
    const double cost = notional + fee;
    const auto quantity = Quantity::create(units);
    if (!quantity || units <= 0.0 || !std::isfinite(cost) || notional <= 0.0 || cost > cash ||
        cost <= fee || (fee > 0.0 && cost <= notional))
        return std::nullopt;
    return quantity;
}

}  // namespace pql::detail
