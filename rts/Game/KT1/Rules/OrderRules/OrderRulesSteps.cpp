/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 OrderRules: the rows of the build and feature steps and of the transport gates.

#include "OrderRulesAdapter.h"
#include "Game/KT1/Core/Common/Rate.h"
#include "Game/KT1/Rules/Orders/OrdersInternal.h"
#include "Sim/Misc/GlobalConstants.h"
#include "Sim/Units/CommandAI/Command.h"
#include "Sim/Units/CommandAI/CommandAI.h"
#include "Sim/Units/Unit.h"
#include "Sim/Units/UnitDef.h"

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

} // namespace

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

bool AskTransportRow(const CompiledRow& row, const CUnit& transporter, const CUnit& cargo, float x, float z,
                     bool engineAllows, int frame) {
	if (row.condition != Condition::TransportGate) return true;
	if (row.callIn == Rows::CallIn::TransportLoad) return Internal::AskTransportLoad(transporter, cargo, engineAllows, frame);
	return Internal::AskTransportUnload(transporter, cargo, x, z, engineAllows, frame);
}

} // namespace KT1::OrderRules
