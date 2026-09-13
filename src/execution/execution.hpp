#pragma once

#include <type_traits>

#include "domain/transaction.hpp"

namespace pql {

enum class ExecutionRejection {
    UnknownSymbol,
    InsufficientCash,
    InsufficientShares,
    InvalidTimestamp,
    DuplicateOrder,
    InvalidArithmetic,
    PortfolioRejected
};

struct ExecutionFill {
    OrderSide side;
    Quantity quantity;
    Price price;
    Money fees;
    Timestamp timestamp;
};

// Allocation-free receipt. (portfolio_id, order_id) identifies the authoritative
// ledger record, which owns the symbol and full financial event.
class Execution {
   public:
    [[nodiscard]] static Execution accepted(const Transaction& transaction) noexcept {
        return Execution{
            transaction.portfolio_id(), transaction.order_id(),
            ExecutionFill{transaction.side(), transaction.quantity(), transaction.price(),
                          transaction.fees(), transaction.timestamp()}};
    }
    [[nodiscard]] static Execution rejected(PortfolioId portfolio, OrderId order,
                                            ExecutionRejection reason) noexcept {
        return Execution{portfolio, order, reason};
    }
    [[nodiscard]] PortfolioId portfolio_id() const noexcept { return portfolio_; }
    [[nodiscard]] OrderId order_id() const noexcept { return order_; }
    [[nodiscard]] bool isFilled() const noexcept { return fill_.has_value(); }
    [[nodiscard]] const std::optional<ExecutionFill>& fill() const noexcept { return fill_; }
    [[nodiscard]] std::optional<ExecutionRejection> rejectionReason() const noexcept {
        return fill_ ? std::nullopt : std::optional{reason_};
    }

   private:
    Execution(PortfolioId portfolio, OrderId order, ExecutionFill fill) noexcept
        : portfolio_(portfolio), order_(order), fill_(fill) {}
    Execution(PortfolioId portfolio, OrderId order, ExecutionRejection reason) noexcept
        : portfolio_(portfolio), order_(order), reason_(reason) {}
    PortfolioId portfolio_;
    OrderId order_;
    std::optional<ExecutionFill> fill_;
    ExecutionRejection reason_{ExecutionRejection::PortfolioRejected};
};

// A completed portfolio mutation must not be followed by a throwing result copy.
static_assert(std::is_nothrow_copy_constructible_v<Execution>);
static_assert(std::is_nothrow_move_constructible_v<Execution>);

}  // namespace pql
