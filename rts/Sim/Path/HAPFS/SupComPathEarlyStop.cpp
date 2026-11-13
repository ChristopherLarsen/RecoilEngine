/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#include "SupComPathEarlyStop.h"

#include "Map/ReadMap.h"
#include "Sim/Misc/GlobalConstants.h"
#include "Sim/MoveTypes/MoveDefHandler.h"
#include "Sim/MoveTypes/MoveMath/MoveMath.h"
#include "System/Config/ConfigHandler.h"
#include "System/SpringMath.h"
#include "System/Threading/ThreadPool.h"

CONFIG(bool, PathEarlyStop).defaultValue(true).description("supcom: a path request whose start is walled in (every square within two of it impassable to the move type) returns no path at once, the same result the full search gives (1); 0 runs the stock search.");

namespace SupComPathEarlyStop {
	static constexpr int RING = 2;                        // squares around the start that must all be impassable
	static constexpr float NEAR_GOAL = 2.0f * 16.0f * SQUARE_SIZE; // two low-res blocks: closer goals are left to the search

	bool On()
	{
		static int on = -1;
		if (on < 0) {
			if (configHandler == nullptr)
				return false;
			on = configHandler->GetBool("PathEarlyStop")? 1: 0;
		}
		return (on != 0);
	}

	bool StartWalledIn(const MoveDef& moveDef, const float3& startPos, const float3& goalPos, float goalRadius, const CSolidObject* owner)
	{
		if (moveDef.allowDirectionalPathing)
			return false;
		if (startPos.SqDistance2D(goalPos) <= Square(goalRadius + NEAR_GOAL))
			return false;

		const int thread = ThreadPool::GetThreadNum();
		const int sx = int(startPos.x / SQUARE_SIZE);
		const int sz = int(startPos.z / SQUARE_SIZE);

		for (int z = sz - RING; z <= sz + RING; z++) {
			for (int x = sx - RING; x <= sx + RING; x++) {
				// outside the map: never entered by a search
				if (x < 0 || z < 0 || x >= mapDims.mapx || z >= mapDims.mapy)
					continue;
				if ((CMoveMath::IsBlockedNoSpeedModCheck(moveDef, x, z, owner, thread) & CMoveMath::BLOCK_STRUCTURE) != 0)
					continue;
				if (CMoveMath::GetPosSpeedMod(moveDef, x, z) == 0.0f)
					continue;
				return false;
			}
		}
		return true;
	}
}
