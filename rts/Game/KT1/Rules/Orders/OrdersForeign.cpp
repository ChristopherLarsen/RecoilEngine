/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 ORDERS: stubs behind OrdersForeign.h (LUA-FREE H01; steps S05, S10 and S12 port them).

#include "OrdersForeign.h"

namespace KT1::Orders {

bool SeenByArmy(const SimContext&, int, int) { return false; }
bool ShieldHasRoom(const SimContext&, int) { return false; }
bool RingPlaceOf(const SimContext&, int, RingPlace&) { return false; }
void SetRingSettled(const SimContext&, int, bool) {}
void SelectLoad(const SimContext&, int, std::span<const int>, int, std::vector<int>&, std::vector<int>&) {}
bool CargoFull(const SimContext&, int) { return false; }
void NoteLeftBehind(const SimContext&, int, int) {}
bool PickupInReach(const SimContext&, int, int) { return false; }
void RemoveWithoutBlast(const SimContext&, int) {}

} // namespace KT1::Orders
