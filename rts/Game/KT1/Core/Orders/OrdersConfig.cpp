/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Orders core: stubs behind OrdersConfig.h (LUA-FREE H01; step S05 ports them).

#include "OrdersConfig.h"

namespace KT1::Core::Orders {

bool SetValue(OrdersConfig&, std::string_view, float) { return false; }
OrdersConfig WithFixture(const OrdersConfig& base, std::string_view) { return base; }

} // namespace KT1::Core::Orders
