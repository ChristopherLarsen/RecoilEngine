/* supcom (PTH-005-H, docs/engine/PTH-005-H-PERF-DESIGN.md) */

#include "SupComPathFast.h"

#include <optional>

#include "Sim/Misc/GlobalSynced.h"
#include "Sim/Misc/GroundBlockingObjectMap.h"
#include "Sim/MoveTypes/MoveDefHandler.h"
#include "Sim/Objects/SolidObject.h"
#include "System/Config/ConfigHandler.h"
#include "System/Log/ILog.h"

CONFIG(int, SupComPathFast).defaultValue(1).minimumValue(0).maximumValue(2).description("supcom: path blocking test without the collision query on empty footprint squares (0 stock, 1 on, 2 on and cross-checked against stock; same results)");

static constexpr int FOOTPRINT_XSTEP = 2;   // as MoveMath.cpp
static constexpr int FOOTPRINT_ZSTEP = 2;

static thread_local bool inStockCheck = false;

static int pathFastMode = -1;
int SupComPathFast::Mode() { if (pathFastMode < 0) pathFastMode = configHandler->GetInt("SupComPathFast"); return pathFastMode; }
bool SupComPathFast::InStockCheck() { return inStockCheck; }

// CMoveMath::IsBlockedNoSpeedModCheckDiff, the same squares in the same order. The stock code builds the collision query (with
// its elevation lookup) and takes a temp number before it reads any square; on open ground every footprint square is empty and
// both are unused. Here they are made at the first non-empty square. The result does not depend on the temp number's value (it
// only marks objects already tested in this call), and an object is only marked after the number is taken, so it stays unique.
static CMoveMath::BlockType FastDiff(const MoveDef& moveDef, int2 prevSqr, int2 newSqr, const CSolidObject* collider, int thread)
{
	const int prev_xmin = std::max(prevSqr.x - moveDef.xsizeh,               -1);
	const int prev_zmin = std::max(prevSqr.y - moveDef.zsizeh,               -1);
	const int prev_xmax = std::min(prevSqr.x + moveDef.xsizeHi, mapDims.mapx - 1);
	const int prev_zmax = std::min(prevSqr.y + moveDef.zsizeHi, mapDims.mapy - 1);

	const int xmin = std::max(newSqr.x - moveDef.xsizeh,                0);
	const int zmin = std::max(newSqr.y - moveDef.zsizeh,                0);
	const int xmax = std::min(newSqr.x + moveDef.xsizeHi, mapDims.mapx - 1);
	const int zmax = std::min(newSqr.y + moveDef.zsizeHi, mapDims.mapy - 1);

	CMoveMath::BlockType ret = CMoveMath::BLOCK_NONE;

	std::optional<MoveTypes::CheckCollisionQuery> colliderInfo;
	int tempNum = 0;

	for (int z = zmin; z <= zmax; z = (z == zmax || z + FOOTPRINT_ZSTEP <= zmax)? z + FOOTPRINT_ZSTEP: zmax) {
		const int zOffset = z * mapDims.mapx;

		for (int x = xmin; x <= xmax; x = (x == xmax || x + FOOTPRINT_XSTEP <= xmax)? x + FOOTPRINT_XSTEP: xmax) {
			if (z <= prev_zmax && z >= prev_zmin && x <= prev_xmax && x >= prev_xmin)
				continue;

			const CGroundBlockingObjectMap::BlockingMapCell& cell = groundBlockingObjectMap.GetCellUnsafeConst(zOffset + x);

			if (cell.size() == 0)
				continue;

			if (!colliderInfo.has_value()) {
				colliderInfo.emplace((collider != nullptr)? MoveTypes::CheckCollisionQuery(collider): MoveTypes::CheckCollisionQuery(&moveDef));
				colliderInfo->UpdateElevationForPos(int2{newSqr.x, newSqr.y});
				tempNum = gs->GetMtTempNum(thread);
			}

			for (size_t i = 0, n = cell.size(); i < n; i++) {
				CSolidObject* collidee = cell[i];

				if (collidee->mtTempNum[thread] == tempNum)
					continue;

				collidee->mtTempNum[thread] = tempNum;

				if (((ret |= CMoveMath::ObjectBlockType(collidee, &*colliderInfo)) & CMoveMath::BLOCK_STRUCTURE) == 0)
					continue;

				return ret;
			}
		}
	}

	return ret;
}

CMoveMath::BlockType SupComPathFast::IsBlockedDiff(const MoveDef& moveDef, int2 prevSqr, int2 newSqr, const CSolidObject* collider, int thread)
{
	const CMoveMath::BlockType fast = FastDiff(moveDef, prevSqr, newSqr, collider, thread);

	if (Mode() == 2) {
		inStockCheck = true;
		const CMoveMath::BlockType stock = CMoveMath::IsBlockedNoSpeedModCheckDiff(moveDef, prevSqr, newSqr, collider, thread);
		inStockCheck = false;

		if (stock != fast) {
			static int mismatches = 0;
			if (++mismatches <= 50)
				LOG_L(L_ERROR, "[SupComPathFast] mismatch %d: movedef %s (%d,%d)->(%d,%d) fast %d stock %d", mismatches, moveDef.name.c_str(), prevSqr.x, prevSqr.y, newSqr.x, newSqr.y, int(fast), int(stock));
			return stock;
		}
	}

	return fast;
}
