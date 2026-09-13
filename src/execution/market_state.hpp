#pragma once

#include <algorithm>
#include <stdexcept>
#include <vector>

#include "domain/market_data.hpp"

namespace pql {

struct MarketAsset {
    Symbol symbol;
    Price price;
    std::vector<PriceBar> history;
};

// Owned decision snapshot. Daily history must precede the UTC decision day.
// Supplied dates are session labels; calendar completeness remains a caller duty.
class MarketState {
   public:
    MarketState(const Symbol& symbol, Price price, Timestamp timestamp)
        : MarketState(timestamp, {{symbol, price, {}}}) {}
    MarketState(Timestamp timestamp, const std::vector<MarketAsset>& assets)
        : assets_(assets), timestamp_(timestamp) {
        if (assets_.empty()) throw std::invalid_argument("Market snapshot requires assets");
        const auto day = std::chrono::floor<std::chrono::days>(timestamp.value());
        for (auto item = assets_.begin(); item != assets_.end(); ++item) {
            if (std::any_of(assets_.begin(), item,
                            [&](const MarketAsset& other) { return other.symbol == item->symbol; }))
                throw std::invalid_argument("Duplicate market symbol");
            std::optional<Date> previous;
            for (const auto& bar : item->history) {
                if (bar.symbol() != item->symbol || (previous && bar.date() <= *previous) ||
                    std::chrono::sys_days{bar.date().value()} >= day) {
                    throw std::invalid_argument("Invalid or future market history");
                }
                previous = bar.date();
            }
        }
    }
    MarketState(const MarketState&) = default;
    MarketState& operator=(const MarketState&) = delete;

    // Legacy single-asset access must never silently choose among multiple quotes.
    [[nodiscard]] const Symbol& symbol() const { return single().symbol; }
    [[nodiscard]] Price price() const { return single().price; }
    [[nodiscard]] Timestamp timestamp() const noexcept { return timestamp_; }
    [[nodiscard]] const std::vector<MarketAsset>& assets() const noexcept { return assets_; }
    [[nodiscard]] const MarketAsset* asset(const Symbol& symbol) const noexcept {
        const auto found =
            std::find_if(assets_.begin(), assets_.end(),
                         [&](const MarketAsset& item) { return item.symbol == symbol; });
        return found == assets_.end() ? nullptr : &*found;
    }

   private:
    [[nodiscard]] const MarketAsset& single() const {
        if (assets_.size() != 1)
            throw std::logic_error("Single-asset access on multiasset snapshot");
        return assets_.front();
    }
    std::vector<MarketAsset> assets_;
    Timestamp timestamp_;
};

}  // namespace pql
