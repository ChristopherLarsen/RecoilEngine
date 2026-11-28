/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 ORDERS: stubs behind OrdersInternal.h (LUA-FREE H01): the row actions (S11). System Orders' state is in
// KT1OrdersTurn.cpp (S05).

#include "OrdersInternal.h"

namespace KT1::Orders::Internal {

void SetFireState(int, Core::Orders::FireState) {}
void NoteGiven(int, const ::Command&) {}
void ToggleSelfDestruct(int, int) {}
void CancelSelfDestruct(int) {}
void SetFerryRoute(int, std::span<const float>, int, int) {}
void EndFerry(int) {}
void SetDive(int, bool) {}
void NoteLivingReclaim(int, int) {}
bool AskTransportLoad(const CUnit&, const CUnit&, bool engineAllows, int) { return engineAllows; }
bool AskTransportUnload(const CUnit&, const CUnit&, float, float, bool engineAllows, int) { return engineAllows; }

} // namespace KT1::Orders::Internal
