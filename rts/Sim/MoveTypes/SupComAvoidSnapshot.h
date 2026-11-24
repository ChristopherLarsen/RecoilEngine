/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

/* SupCom: obstacle avoidance reads the neighbours' state as it was before the parallel traversal pass.
 * CGroundMoveType::GetObstacleAvoidanceDir runs in the parallel UpdateTraversalPlan pass (GroundMoveSystem) and reads the
 * position, velocity, facing and moving bit of the units round it; a neighbour handled by another thread in the same pass
 * could be read before or after that thread touched it, so the avoidance (and the unit's heading) depended on timing
 * (desync 11460, second issue: unit 23243 at frame 10958 turned differently under load). Take() copies those fields for
 * every unit, serially, right before the pass; Of() hands avoidance the copy. Springsetting AvoidSnapshot (default 1; 0
 * reads the live unit, stock). */
#ifndef SUPCOM_AVOID_SNAPSHOT_H
#define SUPCOM_AVOID_SNAPSHOT_H

#include "System/float3.h"

class CSolidObject;

namespace SupComAvoidSnapshot {
	struct View {
		float3 pos;
		float3 speed;
		float3 rightdir;
		float3 frontdir;
		bool moving;
	};

	/// one-line hook in GroundMoveSystem::Update, before the UpdateTraversalPlan pass (main thread)
	void Take();
	bool PlanSplit(); // springsetting AvoidPlanSplit
	/// the avoidee's state for GetObstacleAvoidanceDir: the copy from Take() for a unit, the live object otherwise
	View Of(const CSolidObject* o);
}

#endif
