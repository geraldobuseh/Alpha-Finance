#pragma once

#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "analytics/daily_valuation.hpp"
#include "domain/market_data.hpp"
#include "domain/portfolio.hpp"

namespace pql::persistence {

// Failures are distinct from a successful lookup returning no row. Messages never
// contain SQL, parameter values or connection credentials.
class PersistenceError : public std::runtime_error {
   public:
    using std::runtime_error::runtime_error;
};
class CommitUncertain : public PersistenceError {
   public:
    using PersistenceError::PersistenceError;
};

enum class PortfolioKind { Real, Simulated };
enum class Adjustment { Raw, SplitAdjusted, TotalReturnAdjusted };

struct PriceObservation {
    Symbol symbol;
    Timestamp timestamp;
    std::string source;
    Adjustment adjustment;
    Price open;
    Price high;
    Price low;
    Price close;
    Quantity volume;
    bool operator==(const PriceObservation&) const = default;
};

struct StoredPortfolio {
    std::string name;
    PortfolioKind kind;
    PortfolioSnapshot state;
};

class TransactionRepository {
   public:
    virtual ~TransactionRepository() = default;
    // Original request is required: Trade alone does not retain submission time.
    virtual void append(const Order& request, const Transaction& event) = 0;
    [[nodiscard]] virtual std::vector<Transaction> transactions(PortfolioId id) = 0;
};

class MarketPriceRepository {
   public:
    virtual ~MarketPriceRepository() = default;
    virtual void addAsset(const Symbol& symbol, const std::string& name) = 0;
    virtual void insertPrice(const PriceObservation& observation) = 0;
    struct IngestionCounts {
        std::size_t inserted;
        std::size_t unchanged;
    };
    // Atomic within the owning unit of work. Identical retries are no-ops;
    // conflicting revisions reject instead of overwriting existing observations.
    [[nodiscard]] virtual IngestionCounts storeDailyBars(const Symbol& symbol,
                                                         const std::string& source,
                                                         const std::vector<PriceBar>& bars) = 0;
    [[nodiscard]] virtual std::optional<PriceObservation> price(const Symbol& symbol,
                                                                Timestamp time,
                                                                const std::string& source,
                                                                Adjustment adjustment) = 0;
};

class PortfolioRepository {
   public:
    virtual ~PortfolioRepository() = default;
    [[nodiscard]] virtual PortfolioId createPortfolio(const std::string& name, PortfolioKind kind,
                                                      Money starting_cash) = 0;
    [[nodiscard]] virtual std::optional<StoredPortfolio> portfolio(PortfolioId id) = 0;
    // Metadata only: starting cash and financial history cannot be edited.
    virtual void renamePortfolio(PortfolioId id, const std::string& name) = 0;
};

class ValuationRepository {
   public:
    virtual ~ValuationRepository() = default;
    // Calculates from the authoritative ledger before writing. Caller supplies
    // the immediately preceding exchange session; null is valid only at inception.
    // Same-input retries are no-ops, conflicting revisions/backfills reject.
    // Part of the owning unit of work: commit is required for durability.
    [[nodiscard]] virtual DailyValuation valueDaily(PortfolioId id, Date session, Timestamp close,
                                                    const std::string& source,
                                                    const std::vector<PriceBar>& bars,
                                                    std::optional<Date> previous_session) = 0;
    [[nodiscard]] virtual std::optional<DailyValuation> dailyValuation(PortfolioId id,
                                                                       Date session) = 0;
};

}  // namespace pql::persistence
