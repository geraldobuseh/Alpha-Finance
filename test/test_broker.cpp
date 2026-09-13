// Include first to verify that the public interface is self-contained.
#include "execution/broker.hpp"

// Contract checks and compatibility with the existing Order definition.
#include <type_traits>

#include "domain/order.hpp"

namespace {
using namespace pql;

static_assert(std::is_abstract_v<Broker>);
static_assert(std::has_virtual_destructor_v<Broker>);
static_assert(std::is_nothrow_destructible_v<Broker>);
static_assert(std::is_same_v<decltype(&Broker::execute),
                             Execution (Broker::*)(const Order&, const MarketState&)>);

// Declaration only: verify substitutability without inventing financial result
// types. No object is instantiated and execute is not called in this ticket.
class DeclaredBroker final : public Broker {
   public:
    Execution execute(const Order& order, const MarketState& market) override;
    ~DeclaredBroker() override = default;
};
static_assert(!std::is_abstract_v<DeclaredBroker>);
static_assert(std::is_convertible_v<DeclaredBroker*, Broker*>);
}  // namespace

// All contract checks are compile-time assertions.
int main() { return 0; }
