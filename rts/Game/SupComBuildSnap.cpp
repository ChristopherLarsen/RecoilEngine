/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
#include "SupComBuildSnap.h"
#include "Sim/Misc/GlobalConstants.h"
#include "Sim/Units/UnitDef.h"
#include "System/SpringMath.h"

bool SupComBuildSnap::Fine(const UnitDef* def)
{
	if (def == nullptr)
		return false;
	const auto it = def->customParams.find("supcom_footprint_squares");
	return (it != def->customParams.end() && it->second == "1");
}

float SupComBuildSnap::Axis(float v, int size)
{
	if (size & 1)
		return math::floor(v / SQUARE_SIZE) * SQUARE_SIZE + SQUARE_SIZE * 0.5f;
	return math::floor(v / SQUARE_SIZE + 0.5f) * SQUARE_SIZE;
}
