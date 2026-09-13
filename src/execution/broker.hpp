#pragma once

namespace pql {

class Order;
class MarketState;
class Execution;

// Engine-facing execution boundary, independent of broker SDKs and persistence.
// Defining or calling execute requires a complete Execution; inspecting market
// requires a complete MarketState. Fill and rejection policies are not defined here.
class Broker {
   public:
    [[nodiscard]] virtual Execution execute(const Order& order, const MarketState& market) = 0;
    virtual ~Broker() = default;
};

}  // namespace pql
