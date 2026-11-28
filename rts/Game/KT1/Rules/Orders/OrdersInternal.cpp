/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 ORDERS: stubs behind OrdersInternal.h (LUA-FREE H01; steps S05, S10 and S12 port them).

#include "OrdersInternal.h"

namespace KT1::Orders::Internal {

const Core::Orders::OrdersConfig& Config() {
	static const Core::Orders::OrdersConfig defaults;
	return defaults;
}
Core::Orders::State* State() { return nullptr; }

void ReadQueue(int, int, std::vector<Core::Orders::OrderView>& out) { out.clear(); }
Core::Orders::CommandKind KindOf(int) { return Core::Orders::CommandKind::Other; }
Core::Orders::OrderView ViewOf(const ::Command&) { return {}; }

bool Give(int, int, std::span<const float>, std::uint8_t) { return false; }
bool Remove(int, std::uint32_t) { return false; }
void InsertAhead(int, int, std::span<const float>, Core::Orders::OwnKind, int) {}
void SetMoveGoal(int, float, float, float, float) {}

void Refuse(int, const char*, int) {}

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
