/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
/* SupCom: a landed aircraft's settle step is skipped when it cannot change anything (SIM-CUTS-RANKED #6, 2026-11-24).
 * AAirMoveType::UpdateLanded is a pure function of the unit's position, speed, heading and the terrain under it (dir smoothing
 * 0; the gravity term only runs while above the ground, which a zero speed after the step rules out). After a real call that
 * leaves the speed exactly zero, the result is a fixed point: calling it again with the same inputs writes the same values.
 * Record() keeps those inputs and outputs per unit; Skip() is true only when every one is bitwise unchanged: the ground height
 * under the unit is read again and compared (heights change frame by frame under craters and terraform before any
 * recalculation), and the smooth normal, which changes only in CReadMap::UpdateHeightMapSynced, is guarded by a change stamp
 * per map region (TerrainChanged). So the
 * state is identical with and without the skip on every client, whatever each client's records hold (a loaded game starts
 * with none). No wake hooks: an order, takeoff, transport, impulse, Lua move or turn changes an input or leaves the landed
 * state. PRF-002 bookkeeping (SupComHalfRate::LandedRest) is unchanged. Mod option supcom_air_rest = "0" turns it off. */
#ifndef SUPCOM_AIR_REST_H
#define SUPCOM_AIR_REST_H

class CUnit;
struct SRectangle;

namespace SupComAirRest {
	/// true: this frame's UpdateLanded would change nothing; skip it
	bool Skip(const CUnit* u);
	/// after a real UpdateLanded
	void Record(const CUnit* u);
	/// CReadMap::UpdateHeightMapSynced, with its heightmap rectangle
	void TerrainChanged(const SRectangle& r);
}

#endif
