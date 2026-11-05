/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#include "SupComBombRelease.h"
#include "WeaponDef.h"
#include "Sim/Units/Unit.h"
#include "System/SpringMath.h"

#include <algorithm>

bool SupComBombRelease::Enabled(const WeaponDef* def)
{
	const auto it = def->customParams.find("supcom_bomb_impact");
	return (it != def->customParams.end() && !it->second.empty() && it->second != "0");
}

bool SupComBombRelease::InRange(const CUnit* owner, const float3& dropPos, const float3& tgtPos, float fallTime, float stockWindow)
{
	const float step = owner->speed.Length2D();

	// a carrier with no horizontal speed has no flight line: the stock test
	if (step < 0.001f)
		return (tgtPos.SqDistance2D(dropPos + owner->speed * fallTime) < Square(stockWindow));

	const float3 fwd = (owner->speed * XZVector) / step;
	const float3 rel = (tgtPos - (dropPos + owner->speed * fallTime)) * XZVector;

	const float along = rel.dot(fwd);
	const float cross = (rel - fwd * along).Length2D();

	return (along <= 0.0f && along >= -2.0f * step && cross <= std::max(stockWindow, fallTime));
}
