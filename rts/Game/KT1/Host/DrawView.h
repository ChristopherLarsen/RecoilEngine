/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 host: DrawView, the only door from draw code to the simulation (game-build docs/engine/LUA-FREE-PLAN.md 2.2
// principle 7; SIM-DRAW-PARALLEL.md 13.8). Draw/, UI/ and Audio/ code is handed a const DrawView and includes no Sim/
// header: it gets values and ids, never a pointer to a live object.
//
// What it gives: the draw-side id table (Alive), a copy of a unit's drawn state (Unit), the mirror of the live unit ids
// (UnitIds), the screen ray (Trace) and the snapshots sim systems publish (Snapshots<T>).
//
// Today it reads the live engine (NEW-ENGINE-PLAN agreement: the draw copy, mirrors and id table behind this interface
// are New Engine's to supply, phase 4; until then they are read live, behind the same const interface).
// A missing id means dead: look an id up each frame, never keep what a unit was.

#ifndef KT1_HOST_DRAWVIEW_H
#define KT1_HOST_DRAWVIEW_H

#include <cstdint>
#include <vector>

#include "Game/KT1/Core/DrawHost/IdBits.h"
#include "Game/KT1/Host/Snapshot.h"
#include "System/float3.h"

namespace KT1 {

struct UnitView {
	int id = -1;
	int defId = 0;
	int team = 0;
	int allyTeam = 0;
	float3 drawPos;          // where the engine draws it this frame
	float3 drawMidPos;
	float radius = 0.0f;
	float drawRadius = 0.0f;
	bool inLos = false;      // the local view sees it (always, with a full view)
	bool inRadar = false;
	bool hidden = false;     // not to be drawn: noDraw, or out of the world (transported inside)
	bool icon = false;       // the engine draws its icon in place of the model
};

// what a ray from a screen pixel meets, the rules of Spring.TraceScreenRay
struct RayHit {
	enum Kind : uint8_t { None, Sky, Ground, Unit, Feature };
	Kind kind = None;
	int unitId = -1;
	int featureId = -1;
	bool hasGround = false;
	float3 ground;           // where the ray meets the ground or water (also under a unit or feature)
};

class DrawView {
public:
	// host: once per drawn frame, and at game end
	void BeginFrame(uint32_t drawFrame);
	void Reset();

	int UnitCapacity() const;                       // unit ids are 0 .. capacity - 1
	bool Alive(int id) const;
	bool Unit(int id, UnitView& out) const;         // false: dead or unknown
	const std::vector<int>& UnitIds() const;        // every live unit, ascending, as of this drawn frame
	int Step() const;                               // the sim step the view shows

	// view pixel (x from the left, y from the BOTTOM, as Spring.GetMouseState gives them); None: outside the view
	RayHit Trace(int x, int y) const;

	template<typename T> const Snapshot<T>* Snapshots() const { return SnapshotBoard::Get().Find<T>(); }

private:
	mutable IdBits idBits;
	mutable std::vector<int> unitIds;
	mutable uint32_t idsFrame = 0;
	mutable bool idsValid = false;
	uint32_t frame = 0;
};

} // namespace KT1

#endif
