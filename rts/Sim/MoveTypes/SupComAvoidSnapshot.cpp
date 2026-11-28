/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#include "SupComAvoidSnapshot.h"
#include "System/SupComSimSwitch.h"

#include "Sim/Misc/GlobalSynced.h"
#include "Sim/Objects/SolidObject.h"
#include "Sim/Units/Unit.h"
#include "Sim/Units/UnitHandler.h"
#include "Sim/MoveTypes/MoveType.h"
#include "System/Config/ConfigHandler.h"

#include <vector>

CONFIG(bool, AvoidSnapshot).defaultValue(true).description("supcom: obstacle avoidance reads the neighbours' position, velocity, facing and moving state as they were before the parallel traversal pass (1, the same result on any machine and load); stock reads them live while other threads may be moving them (0).");

namespace {
	struct Entry {
		SupComAvoidSnapshot::View v;
		int frame = -1;
	};

	std::vector<Entry> entries;
	int on = -1;

	bool On()
	{
		if (on < 0) {
			if (configHandler == nullptr)
				return true;
			on = SupComSimSwitch::Get("AvoidSnapshot", true) ? 1 : 0;
		}
		return (on != 0);
	}

	SupComAvoidSnapshot::View Live(const CSolidObject* o)
	{
		const CUnit* u = dynamic_cast<const CUnit*>(o);
		const bool holds = (u != nullptr && u->moveType != nullptr && u->moveType->HoldsAgainstPush());
		return {o->pos, o->speed, o->rightdir, o->frontdir, o->IsMoving(), holds};
	}
}

CONFIG(bool, AvoidPlanSplit).defaultValue(true).description("supcom: the parallel traversal pass runs as three passes (waypoints, then every unit's avoidance, then every unit's speed and heading), so no avoidance reads a neighbour while that neighbour's speed and heading step runs (1, the same result on any machine and load; desync 11460); 0 runs all three per unit in one pass (stock).");

namespace SupComAvoidSnapshot {
	bool PlanSplit()
	{
		static int split = -1;
		if (split < 0)
			split = (configHandler == nullptr || SupComSimSwitch::Get("AvoidPlanSplit", true)) ? 1 : 0;
		return (split != 0);
	}

	void Take()
	{
		if (!On())
			return;

		const int frame = gs->frameNum;
		for (const CUnit* u: unitHandler.GetActiveUnits()) {
			if (u->id < 0)
				continue;
			// supcom (MoveType split L11961: Take was 0.44 ms of 4.14 at 2k): avoidance asks Of() only about an avoidee
			// with a moveDef (GetObstacleAvoidanceDir skips the rest), so a unit without one (structure, aircraft) is never
			// read from the copy: skip it. Same values for every read, so the same result.
			if (u->moveDef == nullptr)
				continue;
			if (static_cast<size_t>(u->id) >= entries.size())
				entries.resize(u->id + 1024);

			const bool holds = (u->moveType != nullptr && u->moveType->HoldsAgainstPush());
			entries[u->id] = {{u->pos, u->speed, u->rightdir, u->frontdir, u->IsMoving(), holds}, frame};   // Live(u) without its dynamic_cast
		}
	}

	View Of(const CSolidObject* o)
	{
		const CUnit* u = dynamic_cast<const CUnit*>(o);
		if (u == nullptr || !On() || u->id < 0 || static_cast<size_t>(u->id) >= entries.size())
			return Live(o);

		const Entry& e = entries[u->id];
		return ((e.frame == gs->frameNum)? e.v: Live(o));
	}
}
