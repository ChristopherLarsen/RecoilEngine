/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Orders core: stubs behind OrdersState.h, and the checks on OrderEvents.h (LUA-FREE H01; step S05 ports them).

#include "OrdersState.h"

#include <type_traits>

#include "OrderEvents.h"
#include "Game/KT1/Core/SimHost/DisplayQueue.h"

namespace KT1::Core::Orders {

static_assert(std::is_trivially_copyable_v<OrderAck> && sizeof(OrderAck) <= SimHost::DISPLAY_TEXT);
static_assert(std::is_trivially_copyable_v<OrderFailed> && sizeof(OrderFailed) <= SimHost::DISPLAY_TEXT);

bool ReachWalk::Has(uint32_t) const { return false; }

void TurnList::Init(int32_t) {}
bool TurnList::Add(UnitId) { return false; }
bool TurnList::Remove(UnitId) { return false; }
bool TurnList::Has(UnitId) const { return false; }

void State::Init(int32_t, int32_t, float, float, const OrdersConfig&) {}
DenseIndex::Slot State::Track(UnitId, int32_t) { return DenseIndex::NONE; }
void State::Forget(UnitId) {}
void State::BeginStep() {}
const DefFacts* State::Def(int32_t) const { return nullptr; }
const DefFacts* State::DefOf(UnitId) const { return nullptr; }

} // namespace KT1::Core::Orders
