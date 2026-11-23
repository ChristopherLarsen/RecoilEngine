/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#include "SupComAvoidSnapshot.h"

#include "Sim/Misc/GlobalSynced.h"
#include "Sim/Objects/SolidObject.h"
#include "Sim/Units/Unit.h"
#include "Sim/Units/UnitHandler.h"
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
			on = configHandler->GetBool("AvoidSnapshot") ? 1 : 0;
		}
		return (on != 0);
	}

	SupComAvoidSnapshot::View Live(const CSolidObject* o)
	{
		return {o->pos, o->speed, o->rightdir, o->frontdir, o->IsMoving()};
	}
}

namespace SupComAvoidSnapshot {
	void Take()
	{
		if (!On())
			return;

		const int frame = gs->frameNum;
		for (const CUnit* u: unitHandler.GetActiveUnits()) {
			if (u->id < 0)
				continue;
			if (static_cast<size_t>(u->id) >= entries.size())
				entries.resize(u->id + 1024);

			entries[u->id] = {Live(u), frame};
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
