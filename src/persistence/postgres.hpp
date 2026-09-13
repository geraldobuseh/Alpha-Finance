#pragma once

#include <memory>

#include "persistence/repositories.hpp"

namespace pql::persistence {

// Trusted orchestration owns one unit per operation; repositories share its DB
// transaction. Strategies receive detached PortfolioSnapshot values only.
// Not thread-safe. Destruction without commit rolls back. Any failed operation
// poisons the whole unit, even if the caller catches the exception.
class PostgresUnitOfWork final : public TransactionRepository,
                                 public MarketPriceRepository,
                                 public PortfolioRepository,
                                 public ValuationRepository {
   public:
    // Empty connection string uses standard libpq PG* environment variables.
    explicit PostgresUnitOfWork(const std::string& connection = "");
    ~PostgresUnitOfWork() override;
    PostgresUnitOfWork(const PostgresUnitOfWork&) = delete;
    PostgresUnitOfWork& operator=(const PostgresUnitOfWork&) = delete;

    void commit();
    void append(const Order& request, const Transaction& event) override;
    [[nodiscard]] std::vector<Transaction> transactions(PortfolioId id) override;
    void addAsset(const Symbol& symbol, const std::string& name) override;
    void insertPrice(const PriceObservation& observation) override;
    [[nodiscard]] IngestionCounts storeDailyBars(const Symbol& symbol, const std::string& source,
                                                 const std::vector<PriceBar>& bars) override;
    [[nodiscard]] std::optional<PriceObservation> price(const Symbol& symbol, Timestamp time,
                                                        const std::string& source,
                                                        Adjustment adjustment) override;
    [[nodiscard]] PortfolioId createPortfolio(const std::string& name, PortfolioKind kind,
                                              Money starting_cash) override;
    [[nodiscard]] std::optional<StoredPortfolio> portfolio(PortfolioId id) override;
    void renamePortfolio(PortfolioId id, const std::string& name) override;
    [[nodiscard]] DailyValuation valueDaily(PortfolioId id, Date session, Timestamp close,
                                            const std::string& source,
                                            const std::vector<PriceBar>& bars,
                                            std::optional<Date> previous_session) override;
    [[nodiscard]] std::optional<DailyValuation> dailyValuation(PortfolioId id,
                                                               Date session) override;

   private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace pql::persistence
