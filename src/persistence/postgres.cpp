#include "persistence/postgres.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <limits>
#include <pqxx/pqxx>

namespace pql::persistence {
namespace {
[[noreturn]] void invalid() { throw PersistenceError("Invalid or inconsistent persistence data"); }

const std::string& textValue(const std::string& value) {
    if (value.find('\0') != std::string::npos) invalid();
    return value;
}

std::string decimal(double value) {
    if (!std::isfinite(value)) invalid();
    if (value == 0) return "0";  // Signed zero has no financial distinction.
    std::array<char, 64> buffer{};
    const auto result = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
    if (result.ec != std::errc{}) invalid();
    return {buffer.data(), result.ptr};
}

double amount(pqxx::work& tx, const pqxx::field& field) {
    const auto input = field.as<std::string>();
    double value{};
    const auto parsed = std::from_chars(input.data(), input.data() + input.size(), value);
    if (parsed.ec != std::errc{} || parsed.ptr != input.data() + input.size() ||
        !std::isfinite(value))
        invalid();
    // Accept only the canonical decimal represented by this engine value. The
    // database compares numerically so harmless trailing zeroes are accepted.
    if (!tx.exec("SELECT $1::numeric = $2::numeric", pqxx::params{input, decimal(value)})[0][0]
             .as<bool>())
        invalid();
    return value;
}

std::int64_t milliseconds(Timestamp time) { return time.value().time_since_epoch().count(); }
int sessionDays(Date date) {
    return static_cast<int>(std::chrono::sys_days{date.value()}.time_since_epoch().count());
}
Date sessionDate(int days) {
    const std::chrono::year_month_day value{std::chrono::sys_days{std::chrono::days{days}}};
    const auto result =
        Date::create(int(value.year()), unsigned(value.month()), unsigned(value.day()));
    if (!result) invalid();
    return *result;
}
std::optional<std::string> returnDecimal(std::optional<double> value) {
    return value ? std::optional<std::string>{decimal(*value)} : std::nullopt;
}
Timestamp timestamp(const pqxx::field& field) {
    return Timestamp{Timestamp::Value{std::chrono::milliseconds{field.as<std::int64_t>()}}};
}
const char* sideName(OrderSide side) {
    switch (side) {
        case OrderSide::Buy:
            return "Buy";
        case OrderSide::Sell:
            return "Sell";
    }
    invalid();
}
const char* kindName(PortfolioKind kind) {
    switch (kind) {
        case PortfolioKind::Real:
            return "real";
        case PortfolioKind::Simulated:
            return "simulated";
    }
    invalid();
}
const char* adjustmentName(Adjustment adjustment) {
    switch (adjustment) {
        case Adjustment::Raw:
            return "raw";
        case Adjustment::SplitAdjusted:
            return "split_adjusted";
        case Adjustment::TotalReturnAdjusted:
            return "total_return_adjusted";
    }
    invalid();
}
// Used only while handling an exception. Never leak libpq diagnostics (which can
// contain SQL, customer values, or connection credentials) through this boundary.
[[noreturn]] void translate() {
    try {
        throw;
    } catch (const pqxx::in_doubt_error&) {
        throw CommitUncertain("Commit outcome unknown; reconcile before retrying");
    } catch (const pqxx::sql_error& error) {
        throw PersistenceError("PostgreSQL operation failed (SQLSTATE " + error.sqlstate() + ")");
    } catch (const pqxx::failure&) {
        throw PersistenceError("PostgreSQL connection or operation failed");
    }
}
}  // namespace

struct PostgresUnitOfWork::Impl {
    pqxx::connection connection;
    pqxx::work tx;
    bool finished{false};
    bool failed{false};

    explicit Impl(const std::string& options) : connection(textValue(options)), tx(connection) {
        tx.exec("SET LOCAL search_path = public, pg_catalog");
        tx.exec("SET LOCAL TIME ZONE 'UTC'");
    }
    void active() const {
        if (failed || finished) throw PersistenceError("Unit of work is no longer usable");
    }
    // Every repository entrypoint shares the failure guard. This template avoids
    // allowing a caught validation exception to accidentally commit prior writes.
    template <class Operation>
    decltype(auto) run(Operation operation) {
        try {
            active();
            return operation();
        } catch (...) {
            failed = true;
            translate();
        }
    }
    pqxx::result lock(PortfolioId id) {
        return tx.exec(
            "SELECT name,kind,starting_cash FROM portfolios WHERE portfolio_id=$1 FOR UPDATE",
            pqxx::params{id.value()});
    }
    std::int64_t asset(const Symbol& symbol) {
        const auto rows =
            tx.exec("SELECT asset_id FROM assets WHERE symbol=$1", pqxx::params{symbol.value()});
        if (rows.empty()) throw PersistenceError("Asset not found");
        return rows[0][0].as<std::int64_t>();
    }
    std::vector<Transaction> history(PortfolioId id) {
        const auto rows = tx.exec(R"SQL(
            SELECT t.order_id,t.symbol,t.side,t.quantity,t.price,t.fees,
                   (extract(epoch FROM t.executed_at)*1000)::bigint AS filled_ms,
                   (extract(epoch FROM e.submitted_at)*1000)::bigint AS submitted_ms
            FROM transactions t JOIN executions e USING (portfolio_id,order_id)
            WHERE t.portfolio_id=$1 ORDER BY t.replay_sequence
        )SQL",
                                  pqxx::params{id.value()});
        std::vector<Transaction> result;
        result.reserve(rows.size());
        for (const auto& row : rows) {
            const auto order_id = OrderId::create(row[0].as<std::int64_t>());
            const auto symbol = Symbol::create(row[1].as<std::string>());
            const auto direction = row[2].as<std::string>();
            if (direction != "Buy" && direction != "Sell") invalid();
            const auto quantity = Quantity::create(amount(tx, row[3]));
            const auto price = Price::create(amount(tx, row[4]));
            const auto fees = Money::create(amount(tx, row[5]));
            if (!order_id || !symbol || !quantity || !price || !fees) invalid();
            const auto order = Order::create_market(
                *order_id, *symbol, direction == "Buy" ? OrderSide::Buy : OrderSide::Sell,
                *quantity, timestamp(row[7]));
            if (!order) invalid();
            const auto trade = Trade::create(*order, *price, timestamp(row[6]));
            if (!trade) invalid();
            const auto event = Transaction::create(id, *trade, *fees);
            if (!event) invalid();
            result.push_back(*event);
        }
        return result;
    }
    Portfolio replay(PortfolioId id, const pqxx::row& seed) {
        const auto cash = Money::create(amount(tx, seed[2]));
        if (!cash) invalid();
        auto result = Portfolio::replay(id, *cash, history(id));
        if (!result) invalid();
        return *result;
    }
    Portfolio atClose(const Portfolio& all, Timestamp close) {
        std::vector<Transaction> prefix;
        for (const auto& event : all.transactionHistory()) {
            if (event.timestamp() > close) break;
            prefix.push_back(event);
        }
        auto result = Portfolio::replay(all.id(), all.startingCash(), prefix);
        if (!result) invalid();
        return *result;
    }
    // Recompute from audited marks and ledger; never trust stored financial totals.
    // Generated SQL total_value is exact decimal cash+market_value. The domain
    // recomputes binary total from the canonical components, avoiding a false
    // precision rejection when a decimal sum has a different double rendering.
    std::vector<DailyValuation> valuations(const Portfolio& all) {
        const auto rows = tx.exec(R"SQL(
            SELECT session_date-DATE '1970-01-01',
                (extract(epoch FROM as_of)*1000)::bigint, ledger_sequence,
                cash,market_value,daily_return,cumulative_return,
                (extract(epoch FROM previous_as_of)*1000)::bigint
            FROM portfolio_snapshots WHERE portfolio_id=$1 AND session_date IS NOT NULL
            ORDER BY session_date
        )SQL",
                                  pqxx::params{all.id().value()});
        std::vector<DailyValuation> result;
        for (const auto& row : rows) {
            const auto date = sessionDate(row[0].as<int>());
            const auto close = timestamp(row[1]);
            const auto stored_marks = tx.exec(R"SQL(
                SELECT symbol,close FROM daily_valuation_marks
                WHERE portfolio_id=$1 AND as_of=TIMESTAMPTZ 'epoch'
                    + ($2::bigint/86400000)::integer*INTERVAL '1 day'
                    + ($2::bigint%86400000)::integer*INTERVAL '1 millisecond'
                ORDER BY symbol COLLATE "C"
            )SQL",
                                              pqxx::params{all.id().value(), milliseconds(close)});
            std::vector<PriceBar> bars;
            for (const auto& mark : stored_marks) {
                const auto symbol = Symbol::create(mark[0].as<std::string>());
                const auto price = Price::create(amount(tx, mark[1]));
                if (!symbol || !price) invalid();
                bars.push_back(PriceBar::create(*symbol, date, *price, *price, *price, *price,
                                                Quantity::create(0).value())
                                   .value());
            }
            const auto state = atClose(all, close);
            auto value = DailyValuation::calculate(state.snapshot(), date, close, bars,
                                                   result.empty() ? nullptr : &result.back());
            const auto sequence = row[2].is_null() ? 0 : row[2].as<std::int64_t>();
            const auto matchesReturn = [&](const pqxx::field& field,
                                           std::optional<double> expected) {
                return expected ? !field.is_null() && amount(tx, field) == *expected
                                : field.is_null();
            };
            if (sequence < 0 || static_cast<std::uint64_t>(sequence) != value.ledgerSequence() ||
                amount(tx, row[3]) != value.cash().value() ||
                amount(tx, row[4]) != value.positionValue().value() ||
                !matchesReturn(row[5], value.dailyReturn()) ||
                !matchesReturn(row[6], value.cumulativeReturn()) ||
                (value.previousAsOf()
                     ? row[7].is_null() || timestamp(row[7]) != *value.previousAsOf()
                     : !row[7].is_null()) ||
                bars.size() != value.marks().size())
                invalid();
            result.push_back(value);
        }
        return result;
    }
};

PostgresUnitOfWork::PostgresUnitOfWork(const std::string& connection) try
    : impl_(std::make_unique<Impl>(connection)) {
} catch (...) {
    translate();
}
PostgresUnitOfWork::~PostgresUnitOfWork() = default;

void PostgresUnitOfWork::commit() {
    impl_->run([&] {
        impl_->tx.commit();
        impl_->finished = true;
    });
}

PortfolioId PostgresUnitOfWork::createPortfolio(const std::string& name, PortfolioKind kind,
                                                Money cash) {
    return impl_->run([&] {
        const auto row = impl_->tx.exec(
            "INSERT INTO portfolios(name,kind,currency,starting_cash) VALUES($1,$2,'USD',$3) "
            "RETURNING portfolio_id",
            pqxx::params{textValue(name), kindName(kind), decimal(cash.value())})[0];
        const auto id = PortfolioId::create(row[0].as<std::int64_t>());
        if (!id) invalid();
        return *id;
    });
}

std::optional<StoredPortfolio> PostgresUnitOfWork::portfolio(PortfolioId id) {
    return impl_->run([&]() -> std::optional<StoredPortfolio> {
        const auto rows = impl_->lock(id);
        if (rows.empty()) return std::nullopt;
        const auto kind = rows[0][1].as<std::string>();
        if (kind != "real" && kind != "simulated") invalid();
        return StoredPortfolio{rows[0][0].as<std::string>(),
                               kind == "real" ? PortfolioKind::Real : PortfolioKind::Simulated,
                               impl_->replay(id, rows[0]).snapshot()};
    });
}

void PostgresUnitOfWork::renamePortfolio(PortfolioId id, const std::string& name) {
    impl_->run([&] {
        if (impl_->tx
                .exec("UPDATE portfolios SET name=$2 WHERE portfolio_id=$1",
                      pqxx::params{id.value(), textValue(name)})
                .affected_rows() != 1)
            throw PersistenceError("Portfolio not found");
    });
}

std::vector<Transaction> PostgresUnitOfWork::transactions(PortfolioId id) {
    return impl_->run([&] {
        const auto rows = impl_->lock(id);
        if (rows.empty()) throw PersistenceError("Portfolio not found");
        // Validate the entire history, not just independently well-formed rows.
        return std::vector<Transaction>{impl_->replay(id, rows[0]).transactionHistory()};
    });
}

void PostgresUnitOfWork::append(const Order& request, const Transaction& event) {
    impl_->run([&] {
        if (request.id() != event.order_id() || request.symbol() != event.symbol() ||
            request.side() != event.side() || request.quantity() != event.quantity() ||
            request.timestamp() > event.timestamp())
            invalid();
        const auto rows = impl_->lock(event.portfolio_id());
        if (rows.empty()) throw PersistenceError("Portfolio not found");
        const auto finalized = impl_->tx.exec(
            R"SQL(
            SELECT 1 FROM portfolio_snapshots WHERE portfolio_id=$1 AND session_date IS NOT NULL
            AND as_of >= TIMESTAMPTZ 'epoch'
                + ($2::bigint/86400000)::integer*INTERVAL '1 day'
                + ($2::bigint%86400000)::integer*INTERVAL '1 millisecond' LIMIT 1
        )SQL",
            pqxx::params{event.portfolio_id().value(), milliseconds(event.timestamp())});
        if (!finalized.empty())
            throw PersistenceError("Trade would invalidate a completed valuation");
        auto state = impl_->replay(event.portfolio_id(), rows[0]);
        if (!state.applyTransaction(event)) invalid();
        const auto last = impl_->tx
                              .exec(
                                  "SELECT coalesce(max(replay_sequence),0) FROM transactions WHERE "
                                  "portfolio_id=$1",
                                  pqxx::params{event.portfolio_id().value()})[0][0]
                              .as<std::int64_t>();
        if (last == std::numeric_limits<std::int64_t>::max()) invalid();
        const auto asset_id = impl_->asset(event.symbol());
        const auto owner = event.portfolio_id().value();
        const auto order = event.order_id().value();
        // Split days/remainder keeps all timestamp arithmetic integral and exact,
        // including negative epoch times; PostgreSQL rejects out-of-range dates.
        impl_->tx.exec(
            R"SQL(
            INSERT INTO orders(portfolio_id,order_id,asset_id,side,quantity,submitted_at)
            VALUES($1,$2,$3,$4,$5,TIMESTAMPTZ 'epoch' + ($6::bigint/86400000)::integer*INTERVAL '1 day'
                + ($6::bigint%86400000)::integer*INTERVAL '1 millisecond')
        )SQL",
            pqxx::params{owner, order, asset_id, sideName(event.side()),
                         decimal(event.quantity().value()), milliseconds(request.timestamp())});
        impl_->tx.exec(
            R"SQL(
            INSERT INTO executions(portfolio_id,order_id,asset_id,side,quantity,submitted_at,price,fees,executed_at)
            SELECT portfolio_id,order_id,asset_id,side,quantity,submitted_at,$3,$4,
                TIMESTAMPTZ 'epoch' + ($5::bigint/86400000)::integer*INTERVAL '1 day'
                + ($5::bigint%86400000)::integer*INTERVAL '1 millisecond'
            FROM orders WHERE portfolio_id=$1 AND order_id=$2
        )SQL",
            pqxx::params{owner, order, decimal(event.price().value()),
                         decimal(event.fees().value()), milliseconds(event.timestamp())});
        impl_->tx.exec(R"SQL(
            INSERT INTO transactions(portfolio_id,replay_sequence,order_id,asset_id,symbol,side,quantity,price,fees,executed_at)
            SELECT portfolio_id,$3,order_id,asset_id,$4,side,quantity,price,fees,executed_at
            FROM executions WHERE portfolio_id=$1 AND order_id=$2
        )SQL",
                       pqxx::params{owner, order, last + 1, event.symbol().value()});
    });
}

void PostgresUnitOfWork::addAsset(const Symbol& symbol, const std::string& name) {
    impl_->run([&] {
        impl_->tx.exec("INSERT INTO assets(symbol,name,currency) VALUES($1,$2,'USD')",
                       pqxx::params{symbol.value(), textValue(name)});
    });
}

void PostgresUnitOfWork::insertPrice(const PriceObservation& p) {
    impl_->run([&] {
        impl_->tx.exec(
            R"SQL(
            INSERT INTO market_prices(asset_id,observed_at,source,adjustment,open,high,low,close,volume)
            VALUES($1,TIMESTAMPTZ 'epoch' + ($2::bigint/86400000)::integer*INTERVAL '1 day'
                + ($2::bigint%86400000)::integer*INTERVAL '1 millisecond',$3,$4,$5,$6,$7,$8,$9)
        )SQL",
            pqxx::params{impl_->asset(p.symbol), milliseconds(p.timestamp), textValue(p.source),
                         adjustmentName(p.adjustment), decimal(p.open.value()),
                         decimal(p.high.value()), decimal(p.low.value()), decimal(p.close.value()),
                         decimal(p.volume.value())});
    });
}

std::optional<PriceObservation> PostgresUnitOfWork::price(const Symbol& symbol, Timestamp time,
                                                          const std::string& source,
                                                          Adjustment adjustment) {
    return impl_->run([&]() -> std::optional<PriceObservation> {
        const auto rows =
            impl_->tx.exec(R"SQL(
            SELECT open,high,low,close,volume FROM market_prices p JOIN assets a USING(asset_id)
            WHERE a.symbol=$1 AND observed_at=TIMESTAMPTZ 'epoch' + ($2::bigint/86400000)::integer*INTERVAL '1 day'
                + ($2::bigint%86400000)::integer*INTERVAL '1 millisecond' AND source=$3 AND adjustment=$4
        )SQL",
                           pqxx::params{symbol.value(), milliseconds(time), textValue(source),
                                        adjustmentName(adjustment)});
        if (rows.empty()) return std::nullopt;
        const auto& row = rows[0];
        const auto open = Price::create(amount(impl_->tx, row[0]));
        const auto high = Price::create(amount(impl_->tx, row[1]));
        const auto low = Price::create(amount(impl_->tx, row[2]));
        const auto close = Price::create(amount(impl_->tx, row[3]));
        const auto volume = Quantity::create(amount(impl_->tx, row[4]));
        if (!open || !high || !low || !close || !volume) invalid();
        return PriceObservation{symbol, time, source, adjustment, *open,
                                *high,  *low, *close, *volume};
    });
}
MarketPriceRepository::IngestionCounts PostgresUnitOfWork::storeDailyBars(
    const Symbol& symbol, const std::string& source, const std::vector<PriceBar>& bars) {
    return impl_->run([&] {
        textValue(source);
        if (source.find_first_not_of(" \t\r\n") == std::string::npos) invalid();
        std::optional<Date> previous;
        for (const auto& bar : bars) {
            if (bar.symbol() != symbol || (previous && bar.date() <= *previous)) invalid();
            previous = bar.date();
        }
        impl_->tx.exec(
            "INSERT INTO assets(symbol,name,currency) VALUES($1::text,$1::text,'USD') ON "
            "CONFLICT(symbol) DO NOTHING",
            pqxx::params{symbol.value()});
        const auto asset =
            impl_->tx.exec("SELECT asset_id,currency FROM assets WHERE symbol=$1 FOR UPDATE",
                           pqxx::params{symbol.value()})[0];
        if (asset[1].as<std::string>() != "USD") invalid();
        const auto asset_id = asset[0].as<std::int64_t>();
        IngestionCounts counts{0, 0};
        for (const auto& bar : bars) {
            const auto days = std::chrono::sys_days{bar.date().value()}.time_since_epoch().count();
            const pqxx::params values{asset_id,
                                      days,
                                      source,
                                      decimal(bar.open().value()),
                                      decimal(bar.high().value()),
                                      decimal(bar.low().value()),
                                      decimal(bar.close().value()),
                                      decimal(bar.volume().value())};
            const auto inserted = impl_->tx.exec(R"SQL(
                INSERT INTO market_prices(asset_id,observed_at,source,adjustment,open,high,low,close,volume,session_date)
                VALUES($1,TIMESTAMPTZ 'epoch'+$2::integer*INTERVAL '1 day',$3,'raw',$4,$5,$6,$7,$8,DATE '1970-01-01'+$2::integer)
                ON CONFLICT DO NOTHING RETURNING asset_id
            )SQL",
                                                 values);
            if (!inserted.empty()) {
                ++counts.inserted;
                continue;
            }
            const auto identical = impl_->tx.exec(R"SQL(
                SELECT 1 FROM market_prices WHERE asset_id=$1 AND session_date=DATE '1970-01-01'+$2::integer
                AND source=$3 AND adjustment='raw' AND open=$4::numeric AND high=$5::numeric
                AND low=$6::numeric AND close=$7::numeric AND volume=$8::numeric
            )SQL",
                                                  values);
            if (identical.empty())
                throw PersistenceError("Conflicting daily market-data revision; batch rejected");
            ++counts.unchanged;
        }
        return counts;
    });
}
}  // namespace pql::persistence

namespace pql::persistence {
std::optional<DailyValuation> PostgresUnitOfWork::dailyValuation(PortfolioId id, Date session) {
    return impl_->run([&]() -> std::optional<DailyValuation> {
        const auto seed = impl_->lock(id);
        if (seed.empty()) throw PersistenceError("Portfolio not found");
        const auto all = impl_->replay(id, seed[0]);
        const auto values = impl_->valuations(all);
        for (const auto& value : values)
            if (value.session() == session) return value;
        return std::nullopt;
    });
}

DailyValuation PostgresUnitOfWork::valueDaily(PortfolioId id, Date session, Timestamp close,
                                              const std::string& source,
                                              const std::vector<PriceBar>& bars,
                                              std::optional<Date> previous_session) {
    return impl_->run([&] {
        textValue(source);
        if (source.find_first_not_of(" \t\r\n\f\v") == std::string::npos)
            throw PersistenceError("Closing-price source is required");
        const auto seed = impl_->lock(id);
        if (seed.empty()) throw PersistenceError("Portfolio not found");
        const auto all = impl_->replay(id, seed[0]);
        const auto values = impl_->valuations(all);
        const DailyValuation* previous = nullptr;
        const DailyValuation* existing = nullptr;
        for (const auto& value : values) {
            if (value.session() < session) previous = &value;
            if (value.session() == session) existing = &value;
        }
        if ((previous && (!previous_session || *previous_session != previous->session())) ||
            (!previous && previous_session)) {
            throw PersistenceError("Missing or inconsistent preceding session valuation");
        }
        if (!existing && !values.empty() && session <= values.back().session())
            throw PersistenceError("Historical valuation insertion requires an explicit rebuild");
        const auto state = impl_->atClose(all, close);
        const auto value =
            DailyValuation::calculate(state.snapshot(), session, close, bars, previous);
        if (existing) {
            const auto saved_source =
                impl_->tx
                    .exec(
                        "SELECT price_source FROM portfolio_snapshots WHERE portfolio_id=$1 "
                        "AND session_date=DATE '1970-01-01'+$2::integer",
                        pqxx::params{id.value(), sessionDays(session)})[0][0]
                    .as<std::string>();
            bool same_marks = existing->marks().size() == value.marks().size();
            for (std::size_t i = 0; same_marks && i < value.marks().size(); ++i) {
                same_marks = existing->marks()[i].symbol == value.marks()[i].symbol &&
                             existing->marks()[i].price == value.marks()[i].price;
            }
            if (existing->asOf() != value.asOf() || saved_source != source || !same_marks ||
                existing->cash() != value.cash() ||
                existing->positionValue() != value.positionValue() ||
                existing->dailyReturn() != value.dailyReturn() ||
                existing->cumulativeReturn() != value.cumulativeReturn() ||
                existing->ledgerSequence() != value.ledgerSequence())
                throw PersistenceError("Conflicting daily valuation revision");
            return value;
        }
        if (value.ledgerSequence() >
            static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()))
            invalid();
        const auto sequence =
            value.ledgerSequence() == 0
                ? std::optional<std::int64_t>{}
                : std::optional<std::int64_t>{static_cast<std::int64_t>(value.ledgerSequence())};
        const auto previous_ms =
            value.previousAsOf() ? std::optional<std::int64_t>{milliseconds(*value.previousAsOf())}
                                 : std::nullopt;
        impl_->tx.exec(
            R"SQL(
            INSERT INTO portfolio_snapshots(portfolio_id,as_of,ledger_sequence,cash,market_value,
                session_date,price_source,daily_return,cumulative_return,previous_as_of)
            VALUES($1,TIMESTAMPTZ 'epoch' + ($2::bigint/86400000)::integer*INTERVAL '1 day'
                + ($2::bigint%86400000)::integer*INTERVAL '1 millisecond', $3,$4,$5,
                DATE '1970-01-01'+$6::integer,$7,$8,$9,
                TIMESTAMPTZ 'epoch' + ($10::bigint/86400000)::integer*INTERVAL '1 day'
                + ($10::bigint%86400000)::integer*INTERVAL '1 millisecond')
        )SQL",
            pqxx::params{id.value(), milliseconds(close), sequence, decimal(value.cash().value()),
                         decimal(value.positionValue().value()), sessionDays(session), source,
                         returnDecimal(value.dailyReturn()),
                         returnDecimal(value.cumulativeReturn()), previous_ms});
        for (const auto& mark : value.marks()) {
            impl_->tx.exec(R"SQL(
                INSERT INTO daily_valuation_marks(portfolio_id,as_of,symbol,close)
                VALUES($1,TIMESTAMPTZ 'epoch' + ($2::bigint/86400000)::integer*INTERVAL '1 day'
                    + ($2::bigint%86400000)::integer*INTERVAL '1 millisecond',$3,$4)
            )SQL",
                           pqxx::params{id.value(), milliseconds(close), mark.symbol.value(),
                                        decimal(mark.price.value())});
        }
        return value;
    });
}
}  // namespace pql::persistence
