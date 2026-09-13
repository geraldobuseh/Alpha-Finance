#pragma once

#include "domain/portfolio.hpp"

namespace pql {

// Strategy-facing name for the existing detached, owning observation value.
// This is not Portfolio and carries no authority to apply trades/transactions.
using PortfolioState = PortfolioSnapshot;

}  // namespace pql
