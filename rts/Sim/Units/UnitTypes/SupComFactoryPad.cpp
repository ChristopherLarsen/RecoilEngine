/* supcom, see SupComFactoryPad.h */

#include "SupComFactoryPad.h"

#include <vector>

#include "Factory.h"
#include "Map/ReadMap.h"
#include "Sim/Misc/GroundBlockingObjectMap.h"
#include "Sim/Units/Unit.h"
#include "Sim/Units/UnitDef.h"
#include "Sim/Units/UnitDefHandler.h"

static float PadClear(const UnitDef* ud)
{
	// per unit def, read once: -1 = no key (stock rule)
	static std::vector<float> cache;
	if (cache.size() <= size_t(ud->id))
		cache.resize(ud->id + 1, -2.0f);
	float& c = cache[ud->id];
	if (c < -1.5f) {
		const auto it = ud->customParams.find("supcom_pad_clear");
		c = (it == ud->customParams.end())? -1.0f: std::max(0.0f, std::stof(it->second));
	}
	return c;
}

bool SupComFactoryPad::Blocked(const CFactory* factory, const float3& buildPos)
{
	const float clear = PadClear(factory->unitDef);

	if (clear < 0.0f)
		return groundBlockingObjectMap.GroundBlocked(buildPos, factory);

	const int x = int(buildPos.x / SQUARE_SIZE);
	const int z = int(buildPos.z / SQUARE_SIZE);
	if (static_cast<unsigned int>(x) >= mapDims.mapx || static_cast<unsigned int>(z) >= mapDims.mapy)
		return false;

	const CGroundBlockingObjectMap::BlockingMapCell& cell = groundBlockingObjectMap.GetCellUnsafeConst(z * mapDims.mapx + x);
	for (size_t i = 0, n = cell.size(); i < n; i++) {
		const CSolidObject* obj = cell[i];
		if (obj == factory)
			continue;
		const CUnit* u = dynamic_cast<const CUnit*>(obj);
		if (u == nullptr || u->immobile)
			return true;
		if (u->pos.SqDistance2D(buildPos) < clear * clear)
			return true;
	}
	return false;
}
