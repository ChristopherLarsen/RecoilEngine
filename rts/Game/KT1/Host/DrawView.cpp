/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 host: DrawView over the live engine (see DrawView.h). The draw host's only file that reads sim objects.

#include "DrawView.h"

#include "Game/Camera.h"
#include "Game/GlobalUnsynced.h"
#include "Game/TraceRay.h"
#include "Game/KT1/Host/Fakes.h"
#include "Rendering/GlobalRendering.h"
#include "Sim/Features/Feature.h"
#include "Sim/Misc/GlobalSynced.h"
#include "Sim/Units/Unit.h"
#include "Sim/Units/UnitDef.h"
#include "Sim/Units/UnitHandler.h"

namespace KT1 {
namespace {

// A fake contact whose type the local view can tell: its own ally team's, the full view, or LuaUtils::IsUnitTyped's rule.
// The unit lists leave it out, as LuaUI's do (gamedata/fake_units.lua); an enemy's unidentified fake stays, like a real one.
bool KnownFake(const CUnit* u) {   // KT1 hot
	if (!Fakes::Is(u)) return false;
	if (gu->spectatingFullView || u->allyteam == gu->myAllyTeam) return true;
	const auto los = u->losStatus[gu->myAllyTeam];
	const int typed = LOS_PREVLOS | LOS_CONTRADAR;
	return (los & LOS_INLOS) != 0 || (los & typed) == typed;
}

} // namespace

void DrawView::BeginFrame(uint32_t drawFrame) { frame = drawFrame; }

void DrawView::Reset() {
	idBits.Init(0);
	unitIds.clear();
	idsValid = false;
}

int DrawView::UnitCapacity() const { return static_cast<int>(unitHandler.MaxUnits()); }

bool DrawView::Alive(int id) const { return id >= 0 && id < UnitCapacity() && unitHandler.GetUnit(id) != nullptr; }

bool DrawView::Unit(int id, UnitView& out) const {   // KT1 hot
	if (!Alive(id)) return false;
	const CUnit* u = unitHandler.GetUnit(id);
	const bool full = gu->spectatingFullView;
	const auto los = u->losStatus[gu->myAllyTeam];
	out.id = id;
	out.defId = u->unitDef->id;
	out.team = u->team;
	out.allyTeam = u->allyteam;
	out.drawPos = u->drawPos;
	out.drawMidPos = u->drawMidPos;
	out.radius = u->radius;
	out.drawRadius = u->GetDrawRadius();
	out.inLos = full || (los & LOS_INLOS) != 0;
	out.inRadar = full || (los & LOS_INRADAR) != 0;
	out.hidden = u->noDraw || u->IsInVoid();
	out.icon = u->GetIsIcon();
	return true;
}

const std::vector<int>& DrawView::UnitIds() const {
	if (!idsValid || idsFrame != frame) {
		if (idBits.Capacity() != UnitCapacity()) {
			idBits.Init(UnitCapacity());
			unitIds.reserve(UnitCapacity());
		}
		idBits.Clear();
		for (const CUnit* u : unitHandler.GetActiveUnits()) {
			if (!KnownFake(u)) idBits.Set(u->id);
		}
		unitIds.clear();
		idBits.AppendTo(unitIds);
		idsFrame = frame;
		idsValid = true;
	}
	return unitIds;
}

int DrawView::Step() const { return gs->frameNum; }

RayHit DrawView::Trace(int x, int y) const {
	RayHit hit;
	if (x < 0 || x >= globalRendering->viewSizeX || y < 0 || y >= globalRendering->viewSizeY) return hit;

	const CUnit* unit = nullptr;
	const CFeature* feature = nullptr;
	// LuaUnsyncedRead::TraceScreenRay's own numbers, so both give the same answer
	const float rawRange = camera->GetFarPlaneDist() * 1.4f;   // DESIGN: the stock engine's value (TraceScreenRay)
	const float badRange = rawRange - 300.0f;                  // DESIGN: the stock engine's value (TraceScreenRay)
	const float3 camPos = camera->GetPos();
	const float3 dir = camera->CalcPixelDir(x + globalRendering->viewPosX, globalRendering->viewSizeY - 1 - y);

	float dist = TraceRay::GuiTraceRay(camPos, dir, rawRange, nullptr, unit, feature, true, false, false);
	if ((dist < 0.0f || dist > badRange) && unit == nullptr && feature == nullptr) {
		hit.kind = RayHit::Sky;
		return hit;
	}
	if (unit != nullptr) {
		hit.kind = RayHit::Unit;
		hit.unitId = unit->id;
	} else if (feature != nullptr) {
		hit.kind = RayHit::Feature;
		hit.featureId = feature->id;
	} else {
		hit.kind = RayHit::Ground;
	}
	if (hit.kind != RayHit::Ground) {   // the same ray again for the ground under the object
		const CUnit* u2 = nullptr;
		const CFeature* f2 = nullptr;
		dist = TraceRay::GuiTraceRay(camPos, dir, rawRange, nullptr, u2, f2, true, true, false);
	}
	hit.hasGround = dist >= 0.0f && dist <= badRange;
	if (hit.hasGround) hit.ground = camPos + (dir * dist);
	return hit;
}

} // namespace KT1
