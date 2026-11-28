/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Orders core: the family's state behind OrdersState.h (the turn list: ord_orders.lua ORDB), and the checks on
// OrderEvents.h. No engine include.

#include "OrdersState.h"

#include <type_traits>

#include "OrderEvents.h"
#include "OrdersConfig.h"
#include "Game/KT1/Core/SimHost/DisplayQueue.h"

namespace KT1::Core::Orders {

static_assert(std::is_trivially_copyable_v<OrderAck> && sizeof(OrderAck) <= SimHost::DISPLAY_TEXT);
static_assert(std::is_trivially_copyable_v<OrderFailed> && sizeof(OrderFailed) <= SimHost::DISPLAY_TEXT);

bool ReachWalk::Has(uint32_t tag) const {
	for (int32_t i = 0; i < count; ++i) {
		if (tags[i] == tag)
			return true;
	}
	return false;
}

void TurnList::Init(int32_t maxUnits) {
	list.clear();
	list.reserve(static_cast<size_t>(maxUnits));
	at.assign(static_cast<size_t>(maxUnits), -1);   // DESIGN: -1, not in the list
}

bool TurnList::Add(UnitId unit) {
	if (unit < 0 || unit >= static_cast<UnitId>(at.size()) || at[unit] >= 0)
		return false;
	at[unit] = Size();
	list.push_back(unit);
	return true;
}

// the Lua's ORDB.Del: the last unit takes the leaver's slot
bool TurnList::Remove(UnitId unit) {
	if (!Has(unit))
		return false;
	const int32_t i = at[unit];
	const UnitId last = list.back();
	list[i] = last;
	at[last] = i;
	list.pop_back();
	at[unit] = -1;   // DESIGN: after the line above, so a leaver that was last ends up out
	return true;
}

bool TurnList::Has(UnitId unit) const {
	return unit >= 0 && unit < static_cast<UnitId>(at.size()) && at[unit] >= 0;
}

namespace {

void OwnTurnColumns(State& s) {
	s.units.Own(s.defId);
	s.units.Own(s.frontClass);
	s.units.Own(s.nextCheck, NO_STEP);
	s.units.Own(s.own);
	s.units.Own(s.stripped, NO_STEP);
	s.units.Own(s.guardRepair, NO_UNIT);
	s.units.Own(s.guardShield, NO_UNIT);
	s.units.Own(s.given);
	s.units.Own(s.stall);
	s.units.Own(s.walk);
}

void OwnQueueColumns(State& s) {
	s.units.Own(s.move);
	s.units.Own(s.fresh, NO_STEP);
	s.units.Own(s.stop);
	s.units.Own(s.failedAt, NO_STEP);
	s.units.Own(s.waypointDue);
	s.units.Own(s.waypointStep);
	s.units.Own(s.goalCells);
	s.units.Own(s.fireState, static_cast<uint8_t>(FireState::Free));   // the Lua reads an unset fire_state as free
	s.units.Own(s.selfDestruct, NO_STEP);
	s.units.Own(s.livingReclaim, NO_UNIT);
	s.units.Own(s.orderClock);
	s.units.Own(s.loading);
	s.units.Own(s.unloading);
	s.units.Own(s.dropHold);
	s.units.Own(s.reverseHold);
	s.units.Own(s.loadOrder);
	s.units.Own(s.ferry);
}

void InitLists(State& s, int32_t maxUnits) {
	for (SortedIdList* list: {&s.moveUnits, &s.freshUnits, &s.shieldUnits, &s.waypointUnits, &s.selfDestructUnits,
	                          &s.reclaimUnits, &s.ferryUnits})
		list->Init(maxUnits);
	s.turn.Init(maxUnits);
	s.turnThisStep.clear();
	s.turnThisStep.reserve(static_cast<size_t>(maxUnits));
	s.issued.clear();
	s.fallbacks.clear();
	s.failures.clear();
}

} // namespace

void State::Init(int32_t maxUnits, int32_t defCount, float mapX, float mapZ, const OrdersConfig& config) {
	units.Init(maxUnits);
	OwnTurnColumns(*this);
	OwnQueueColumns(*this);
	InitLists(*this, maxUnits);
	// DESIGN: the change grid keeps a margin of one query reach round the map
	changes.Init(mapX, mapZ, config.refill.cell_elmo, config.refill.near_elmo);
	structures.Init(mapX, mapZ, config.boxed.cell_elmo, maxUnits);
	defs.assign(static_cast<size_t>(defCount) + 1, DefFacts{});   // def ids start at 1
	settingFire = false;
	givingFerryOrder = false;
	fillSquaresUsed = 0;
	routeEngine = -1;   // DESIGN: -1, not known yet
}

DenseIndex::Slot State::Track(UnitId unit, int32_t defID) {
	const DenseIndex::Slot known = units.index.SlotOf(unit);
	if (known != DenseIndex::NONE)
		return known;
	const DenseIndex::Slot slot = units.AddUnit(unit);
	if (slot != DenseIndex::NONE)
		defId[slot] = defID;
	return slot;
}

void State::Forget(UnitId unit) {
	turn.Remove(unit);
	for (SortedIdList* list: {&moveUnits, &freshUnits, &shieldUnits, &waypointUnits, &selfDestructUnits, &reclaimUnits,
	                          &ferryUnits})
		list->Erase(unit);
	structures.Remove(unit);
	units.RemoveUnit(unit);
}

void State::BeginStep() {
	turnThisStep.clear();
	issued.clear();
	fallbacks.clear();
}

const DefFacts* State::Def(int32_t defID) const {
	if (defID <= 0 || defID >= static_cast<int32_t>(defs.size()))
		return nullptr;
	return &defs[defID];
}

const DefFacts* State::DefOf(UnitId unit) const {
	const DenseIndex::Slot slot = units.index.SlotOf(unit);
	return (slot == DenseIndex::NONE) ? nullptr : Def(defId[slot]);
}

} // namespace KT1::Core::Orders
