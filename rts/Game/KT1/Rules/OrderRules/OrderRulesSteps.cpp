/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 OrderRules: the rows of the build and feature steps and of the transport gates.

#include <string>
#include <vector>

#include "OrderRulesAdapter.h"
#include "Game/KT1/Core/Common/Rate.h"
#include "Game/KT1/Core/Orders/QueueHandling.h"
#include "Game/KT1/Host/Fakes.h"
#include "Game/KT1/Rules/Orders/OrdersInternal.h"
#include "Sim/Misc/GlobalConstants.h"
#include "Sim/Misc/GlobalSynced.h"
#include "Sim/Units/CommandAI/Command.h"
#include "Sim/Units/CommandAI/CommandAI.h"
#include "Sim/Units/Unit.h"
#include "Sim/Units/UnitDef.h"
#include "System/EventClient.h"
#include "System/EventHandler.h"
#include "System/float3.h"

namespace KT1::OrderRules {
namespace {

using Rows::CompiledRow;
using Rows::Condition;

namespace Internal = Orders::Internal;

// the builder's current order is the player's one-target reclaim of this unit
bool DirectReclaim(const CUnit& builder, const CUnit& target) {
	const auto& queue = builder.commandAI->commandQue;
	if (queue.empty()) return false;
	const ::Command& c = queue.front();
	return c.GetID() == CMD_RECLAIM && c.GetNumParams() == 1 && static_cast<int>(c.GetParam(0)) == target.id
		&& !c.IsInternalOrder();
}

// ECO-019: a step that takes apart a unit no order of the player names, or one that may not be taken
bool Living(const CUnit& builder, const CUnit& target) {
	const auto& defs = Internal::Defs();
	const int def = target.unitDef->id;
	const bool reclaimable = def > 0 && def < static_cast<int>(defs.size())
		&& defs[def].Has(Core::Orders::DefFlag::Reclaimable);
	return target.id == builder.id || !reclaimable || (!DirectReclaim(builder, target) && !target.beingBuilt);
}

// ord_pickup.lua InReach: a transport with a pickup reach takes only a unit within it, horizontally
bool InPickupReach(const Table& table, const CUnit& transporter, const CUnit& cargo) {
	const int def = transporter.unitDef->id;
	if (def < 0 || def >= static_cast<int>(table.pickupReach.size()) || table.pickupReach[def] <= 0) return true;
	const float reach = table.pickupReach[def];
	return transporter.pos.distance2D(cargo.pos) <= reach;
}

// ord_orders.lua UnitCommand: when a transport's load or unload order was given, which its gate counts from
void NoteTransportCommand(const CUnit& u, const ::Command& c) {
	Core::Orders::State* state = Internal::State();
	if (state == nullptr) return;
	const int id = c.GetID();
	const bool pickup = id == Core::Orders::CUSTOM_PICKUP;
	const bool load = pickup || id == CMD_LOAD_UNITS || id == CMD_LOAD_ONTO;
	if (!load && id != CMD_UNLOAD_UNITS && id != CMD_UNLOAD_UNIT) return;
	std::vector<Core::Orders::OrderView> queue;
	Internal::ReadQueue(u.id, 1, queue);
	const bool behind = (c.GetOpts() & SHIFT_KEY) != 0 && !queue.empty();
	const auto slot = state->Track(u.id, u.unitDef->id);
	Core::Orders::NoteTransportOrder(state->orderClock[slot],
		load ? Core::Orders::ClockKind::Load : Core::Orders::ClockKind::Unload, gs->frameNum, pickup, behind);
}

class TransportOrders final : public CEventClient {
public:
	TransportOrders() : CEventClient("[KT1OrderRules]", CLIENT_ORDER, true) {}
	bool WantsEvent(const std::string& name) override { return name == "UnitCommand"; }
	int GetReadAllyTeam() const override { return AllAccessTeam; }
	void UnitCommand(const CUnit* u, const ::Command& c, int, bool, bool) override {
		if (!Fakes::Is(u)) NoteTransportCommand(*u, c);   // a fake's call-ins never reach ord_orders.lua
	}
};

} // namespace

void RegisterTransportOrders() {
	// not a static object: CEventClient's destructor calls the event handler, which may be gone by process exit
	static TransportOrders* client = nullptr;
	if (client == nullptr) client = new TransportOrders();
	else eventHandler.RemoveClient(client);
	eventHandler.AddClient(client);
}

bool AskStepRow(Table& table, const CompiledRow& row, const CUnit& builder, const CUnit* target, float part, int frame) {
	if (part >= 0) return true;   // building and repairing pass every row
	switch (row.condition) {
		case Condition::LivingReclaim:
			if (target != nullptr && Living(builder, *target)) Internal::NoteLivingReclaim(builder.id, target->id);
			return true;
		case Condition::ReclaimWhileMoving: {
			const Rows::GuardValues& v = table.config.values;
			const auto slot = table.state.Track(builder.id);
			const bool still = Rows::Still(builder.speed.x, builder.speed.z, v.reclaim_still_elmo_s / GAME_SPEED);
			return Rows::AskReclaim(table.state.reclaim[slot], still, frame, Core::Rate::Every(v.reclaim_latency_s));
		}
		default: return true;
	}
}

bool AskTransportRow(const Table& table, const CompiledRow& row, const CUnit& transporter, const CUnit& cargo,
                     float x, float z, bool engineAllows, int frame) {
	if (row.condition != Condition::TransportGate) return true;
	if (row.callIn != Rows::CallIn::TransportLoad)
		return Internal::AskTransportUnload(transporter, cargo, x, z, engineAllows, frame);
	if (InPickupReach(table, transporter, cargo)) return Internal::AskTransportLoad(transporter, cargo, engineAllows, frame);
	// out of reach: the load time starts over when it is in reach
	Core::Orders::State* state = Internal::State();
	const auto slot = (state != nullptr) ? state->SlotOf(transporter.id) : Core::DenseIndex::NONE;
	if (slot != Core::DenseIndex::NONE) state->loading[slot] = Core::Orders::GateClock{};
	return false;
}

} // namespace KT1::OrderRules
