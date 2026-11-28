/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 ORDERS: stubs behind OrdersTargetPick.h (LUA-FREE H01; steps S03 and S04 port them).

#include "OrdersTargetPick.h"

namespace KT1::Orders {

AMovePick PickTarget(const SimContext&, int, const AMovePickQuery&) { return {}; }
EngageAnswer Engage(const SimContext&, int, int, bool, int) { return {}; }
void SetFocus(const SimContext&, int, int) {}
int ScanSteps(const SimContext&, int) { return 0; }

} // namespace KT1::Orders
