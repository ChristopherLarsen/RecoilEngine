/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 OrderRules: the work of the guard gadgets between orders (LUA-FREE S14): idle aircraft over water
// (mov_air_idle_water.lua), the cargo of a short set-down (mov_air_unload_gap.lua UnitUnloaded) and the orders of a
// unit that changes owner (ord_transfer.lua).
//
// Timing differs from the Lua: it acts inside UnitGiven and UnitUnloaded, here the events are handled in the same step,
// after the units' updates (an event of the step's start is handled there too).

#include <algorithm>
#include <array>
#include <vector>

#include "OrderRulesAdapter.h"
#include "Game/GameSetup.h"
#include "Game/KT1/Core/Common/Rate.h"
#include "Game/KT1/Core/Orders/DefFacts.h"
#include "Game/KT1/Host/Phases.h"
#include "Game/KT1/Host/SimContext.h"
#include "Game/KT1/Host/SyncState.h"
#include "Game/KT1/Rules/Orders/OrdersInternal.h"
#include "Map/Ground.h"
#include "Sim/MoveTypes/AAirMoveType.h"
#include "Sim/Units/CommandAI/Command.h"
#include "Sim/Units/CommandAI/CommandAI.h"
#include "Sim/Units/CommandAI/FactoryCAI.h"
#include "Sim/Units/Unit.h"
#include "Sim/Units/UnitDef.h"
#include "Sim/Units/UnitHandler.h"
#include "Sim/Units/UnitTypes/Builder.h"
#include "Sim/Units/UnitTypes/Factory.h"
#include "System/float3.h"
#include "System/Log/ILog.h"

namespace KT1::OrderRules {
namespace {

using Core::SimHost::Event;
using Core::SimHost::EventType;
using Core::SimHost::MaskOf;
using Core::SimHost::Phase;
using Slot = Core::DenseIndex::Slot;

namespace Internal = Orders::Internal;

constexpr int32_t LAND_WHEN_IDLE = 1;    // DESIGN (engine fact): idle mode 1, an idle aircraft lands
constexpr int32_t KEEP_FLYING = 0;       // DESIGN (engine fact): idle mode 0

const Core::Orders::DefFacts* FactsOf(const CUnit& u) {
	const auto& defs = Internal::Defs();
	const int id = u.unitDef->id;
	return (id > 0 && id < static_cast<int>(defs.size())) ? &defs[id] : nullptr;
}

// ---- idle aircraft over water ----------------------------------------------------------------------------------------

void SetIdleMode(Table& table, int unit, int32_t mode) {
	const std::array<float, 1> param{static_cast<float>(mode)};
	table.state.settingIdleMode = true;
	Internal::Give(unit, CMD_IDLEMODE, param, 0);
	table.state.settingIdleMode = false;
}

// KT1 hot
void LookAtLanders(Table& table, const SimContext& c) {
	if (!table.config.air_idle_water || c.frame % Core::Rate::Every(table.config.values.idle_water_check_s) != 0) return;
	for (const int32_t id : table.state.landers.Ids()) {
		const CUnit* u = unitHandler.GetUnit(id);
		const Slot slot = table.state.SlotOf(id);
		if (u == nullptr || slot == Core::DenseIndex::NONE || !c.ids.Contains(id)) continue;
		const bool water = CGround::GetHeightReal(u->pos.x, u->pos.z, true) < 0;
		Rows::IdleAir& air = table.state.idleAir[slot];
		if (water && !air.held) {
			const auto* flier = dynamic_cast<const AAirMoveType*>(u->moveType);
			air.mode = (flier != nullptr && !flier->autoLand) ? KEEP_FLYING : LAND_WHEN_IDLE;
			if (air.mode != LAND_WHEN_IDLE) continue;
			SetIdleMode(table, id, KEEP_FLYING);
			air.held = true;
		} else if (!water && air.held) {
			air.held = false;
			SetIdleMode(table, id, air.mode);
		}
	}
}

// ---- cargo of a short set-down ---------------------------------------------------------------------------------------

void SetDownShort(Table& table, const SimContext& c, int cargo, int transport) {
	const Slot slot = table.state.SlotOf(transport);
	CUnit* u = unitHandler.GetUnit(cargo);
	if (u == nullptr || slot == Core::DenseIndex::NONE || !table.state.unload[slot].active) return;
	const Rows::UnloadPending& to = table.state.unload[slot];
	const float3 pos(to.x, CGround::GetHeightReal(to.x, to.z, true), to.z);
	c.Write([u, pos]() { u->ForcedMove(pos); });   // KT1 lint: allow direct-write inside SimContext::Write
}

// the last of a batch ends it
void EndEmptyBatch(Table& table, int transport) {
	const Slot slot = table.state.SlotOf(transport);
	const CUnit* u = unitHandler.GetUnit(transport);
	if (slot != Core::DenseIndex::NONE && (u == nullptr || u->transportedUnits.empty())) table.state.unload[slot] = {};
}

// ---- a unit that changes owner (ORD-012 v3) --------------------------------------------------------------------------

const CCommandQueue* FactoryQueue(const CUnit& u) {
	const CFactoryCAI* factory = dynamic_cast<const CFactoryCAI*>(u.commandAI);
	return (factory == nullptr) ? nullptr : &factory->newUnitCommands;
}

int32_t Builds(const CUnit& u) {
	const CCommandQueue* q = FactoryQueue(u);
	if (q == nullptr) return 0;
	return static_cast<int32_t>(std::count_if(q->begin(), q->end(), [](const ::Command& c) { return c.GetID() < 0; }));
}

CUnit* BuiltFrame(CUnit& u) {
	if (auto* builder = dynamic_cast<CBuilder*>(&u); builder != nullptr && builder->curBuild != nullptr) return builder->curBuild;
	auto* factory = dynamic_cast<CFactory*>(&u);
	return (factory != nullptr) ? factory->curBuild : nullptr;
}

void RemoveByTag(int unit, const std::vector<float>& tags) {
	// one order for each tag: the removals of one Spring order
	for (const float tag : tags) {
		const std::array<float, 1> param{tag};
		Internal::Give(unit, CMD_REMOVE, param, 0);
	}
}

// R6: every queued item but the one in progress; the frame goes with its factory
void TrimFactory(const SimContext& c, CUnit& u, int newTeam) {
	const CCommandQueue* q = FactoryQueue(u);
	if (q == nullptr) return;
	CUnit* frame = BuiltFrame(u);
	std::vector<float> tags;   // entries that are not builds go by tag: a right-click on them removes nothing
	for (const ::Command& command : *q) {
		if (command.GetID() >= 0) tags.push_back(static_cast<float>(command.GetTag()));
	}
	const int32_t builds = Builds(u);
	if (!tags.empty()) RemoveByTag(u.id, tags);
	const int32_t keep = Rows::TransferKeep(frame != nullptr, builds);
	for (int32_t i = 0; i < builds - keep; ++i) {
		int32_t left = 0;
		int last = 0;
		for (const ::Command& command : *q) {
			if (command.GetID() >= 0) continue;
			++left;
			last = command.GetID();
		}
		if (left == 0 || left <= keep) break;
		Internal::Give(u.id, last, {}, RIGHT_MOUSE_KEY);
	}
	if (frame != nullptr && frame->team != newTeam)
		c.Write([frame, newTeam]() { frame->ChangeTeam(newTeam, CUnit::ChangeGiven); });   // KT1 lint: allow direct-write inside SimContext::Write
}

// R5: other units' attack, guard and repair orders that name it end
void EndAimedOrders(int target) {
	std::vector<float> tags;
	for (const CUnit* other : unitHandler.GetActiveUnits()) {
		if (other->id == target) continue;
		tags.clear();
		for (const ::Command& command : other->commandAI->commandQue) {
			const int id = command.GetID();
			const bool aimed = id == CMD_ATTACK || id == CMD_GUARD || id == CMD_REPAIR;
			if (aimed && command.GetNumParams() == 1 && static_cast<int>(command.GetParam(0)) == target)
				tags.push_back(static_cast<float>(command.GetTag()));
		}
		if (!tags.empty()) RemoveByTag(other->id, tags);
	}
}

void ChangedOwner(Table& table, const SimContext& c, int unit, int newTeam) {
	CUnit* u = unitHandler.GetUnit(unit);
	if (u == nullptr || Internal::State() == nullptr) return;   // the fire state is Orders'
	const Core::Orders::DefFacts* facts = FactsOf(*u);
	if (facts == nullptr) return;
	if (u->unitDef->IsFactoryUnit()) {
		const CCommandQueue* q = FactoryQueue(*u);
		const int32_t keep = (BuiltFrame(*u) != nullptr && q != nullptr && !q->empty()) ? 1 : 0;
		TrimFactory(c, *u, newTeam);
		// the engine may still be moving the factory between teams: the trim is repeated until only the item stays
		if (Builds(*u) > keep) {
			const Slot slot = table.state.Track(unit);
			table.state.transfer[slot] = {true, newTeam, keep, 0};
			table.state.transfers.Insert(unit);
		}
	} else {
		Internal::Give(unit, CMD_STOP, {}, 0);
	}
	Internal::SetFireState(unit, Core::Orders::DefaultFireState(*facts, Internal::Config().fire));
	EndAimedOrders(unit);
}

// KT1 hot
void RepeatTrims(Table& table, const SimContext& c) {
	if (table.state.transfers.Ids().empty()) return;
	const std::vector<int32_t> ids(table.state.transfers.Ids().begin(), table.state.transfers.Ids().end());
	for (const int32_t id : ids) {
		if (!c.ids.Contains(id)) continue;
		CUnit* u = unitHandler.GetUnit(id);
		const Slot slot = table.state.SlotOf(id);
		bool done = u == nullptr || slot == Core::DenseIndex::NONE || u->team != table.state.transfer[slot].team;
		if (!done) {
			Rows::TransferPending& p = table.state.transfer[slot];
			TrimFactory(c, *u, p.team);
			const int32_t left = Builds(*u);
			p.tries += 1;
			done = left <= p.keep || p.tries >= table.config.values.transfer_trim_tries;
			if (done && left > p.keep)
				LOG("[KT1] %s: factory %d still holds %d queued items after the trim", NAME, id, left);
		}
		if (!done) continue;
		if (slot != Core::DenseIndex::NONE) table.state.transfer[slot] = {};
		table.state.transfers.Erase(id);
	}
}

// ---- events ----------------------------------------------------------------------------------------------------------

void TakeEvents(Table& table, const SimContext& c) {
	std::vector<int32_t> unloadedFrom;
	c.events.ForEachRaised([&](const Event& e) {
		if (!c.ids.Contains(e.id)) return;
		switch (e.type) {
			case EventType::UnitCreated: {
				const CUnit* u = unitHandler.GetUnit(e.id);
				const Core::Orders::DefFacts* facts = (u != nullptr) ? FactsOf(*u) : nullptr;
				if (facts == nullptr || !facts->Has(Core::Orders::DefFlag::IdleLands)) return;
				table.state.Track(e.id);
				table.state.landers.Insert(e.id);
				break;
			}
			case EventType::UnitDestroyed: table.state.Forget(e.id); break;
			case EventType::UnitUnloaded:
				SetDownShort(table, c, e.id, e.other);
				unloadedFrom.push_back(e.other);
				break;
			case EventType::UnitGiven: ChangedOwner(table, c, e.id, e.arg2); break;
			default: break;
		}
	});
	for (const int32_t transport : unloadedFrom) EndEmptyBatch(table, transport);
}

// the state a later step's decision reads, in the sync checksum
void HashState(Table& table, Core::SimHost::StateHash& h) {
	for (const int32_t id : table.state.units.index.AscendingIds()) {
		const Slot slot = table.state.SlotOf(id);
		const Rows::ReclaimStill& still = table.state.reclaim[slot];
		const Rows::IdleAir& air = table.state.idleAir[slot];
		const Rows::UnloadPending& unload = table.state.unload[slot];
		const Rows::TransferPending& transfer = table.state.transfer[slot];
		h.Add(id);
		h.Add(still.last);
		h.Add(still.since);
		h.Add(static_cast<int32_t>(air.held));
		h.Add(air.mode);
		h.Add(static_cast<int32_t>(unload.active));
		h.Add(unload.x);
		h.Add(unload.z);
		h.Add(static_cast<int32_t>(transfer.active));
		h.Add(transfer.tries);
	}
}

} // namespace

void RegisterFrameWork(Table& table) {
	const auto* options = &CGameSetup::GetModOptions();
	const auto water = options->find("air_idle_water");
	if (water != options->end() && water->second == "0") table.config.air_idle_water = false;
	constexpr auto EVENTS = MaskOf(EventType::UnitCreated) | MaskOf(EventType::UnitDestroyed)
		| MaskOf(EventType::UnitUnloaded) | MaskOf(EventType::UnitGiven);
	Phases::Add(NAME, Phase::FrameStart, Orders::PHASE_IDLE_WATER, 0, [&table](const SimContext& c) {
		table.state.BeginStep();
		LookAtLanders(table, c);
	});
	Phases::Add(NAME, Phase::AfterUnits, Orders::PHASE_IDLE_WATER, EVENTS,
		[&table](const SimContext& c) { TakeEvents(table, c); });
	Phases::Add(NAME, Phase::FrameStart, Orders::PHASE_TRANSFER, 0,
		[&table](const SimContext& c) { RepeatTrims(table, c); });
	SyncState::Add(NAME, "guards", [&table](Core::SimHost::StateHash& h) { HashState(table, h); });
}

} // namespace KT1::OrderRules
