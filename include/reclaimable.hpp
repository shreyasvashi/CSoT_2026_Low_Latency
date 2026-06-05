#pragma once
#include <vector>
#include "strategy.hpp"

namespace csot {

struct IReclaimable {
    virtual void reclaim_orders(std::vector<Order>&&) noexcept = 0;
protected:
    ~IReclaimable() = default;
};

} // namespace csot
