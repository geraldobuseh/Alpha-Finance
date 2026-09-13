#pragma once

#include "domain/financial_types.hpp"

namespace pql {

// One configured synthetic quote available at the supplied execution time.
// Owns its symbol; no network, wall-clock reads or historical availability claims.
class MarketState {
   public:
    MarketState(const Symbol& symbol, Price price, Timestamp timestamp)
        : symbol_(symbol), price_(price), timestamp_(timestamp) {}
    [[nodiscard]] const Symbol& symbol() const noexcept { return symbol_; }
    [[nodiscard]] Price price() const noexcept { return price_; }
    [[nodiscard]] Timestamp timestamp() const noexcept { return timestamp_; }

   private:
    Symbol symbol_;
    Price price_;
    Timestamp timestamp_;
};

}  // namespace pql
