/* SupCom: idle units update their movement every other frame (owner decision 2026-11-20, replacing profile cut G).
 * Game key: mod option supcom_half_rate; on unless it is "0" (then stock, every frame).
 * PRF-002 v1: an idle unit updates only on ticks where tick + id is even (period 2); it is idle from the tick after an
 * update that shows no change, and anything that changes it (order, damage, a push, a path) wakes it for the next tick.
 *   ground: idle = no orders, not hit last tick, not carried, built, stunned or skidding, standing still with no path, no
 *     pending path event and no collision push; the pre-collision step is skipped on its off ticks;
 *   air: only an aircraft at rest on the ground (LandedRest); a flying or hovering aircraft changes every update and is
 *     updated every tick (R6). All tests read synced state only, so
 * every client makes the same choice. Every 300 frames the share on half rate is logged ([HALFRATE]). */
#ifndef SUPCOM_HALF_RATE_H
#define SUPCOM_HALF_RATE_H

class CUnit;
struct lua_State;

namespace SupComHalfRate {
	/// one-line hook in GroundMoveSystem before UpdatePreCollisions: true = skip it this frame
	bool SkipGround(CUnit* u);
	/// PRF-002 v1 R1 for the rest of a ground unit's movement update: true = an idle unit's off tick, so its traversal plan
	/// (the parallel pass) and its serial move type update are skipped this frame. Decided once per frame at the plan pass
	/// (stored in the unit, read again by the serial update); synced state only. Switch: mod option supcomhalfrateplan
	/// (default on; 0: only the pre-collision step is on half rate, as before).
	bool PlanOffTick(CUnit* u);
	/// DESIGN (far-view proposal, coordinator 2026-11-26; beyond PRF-002 v1's period 2): a ground unit idle by the test of
	/// PlanOffTick and not moved for SLEEP_FRAMES frames in a row sleeps: plan, pre-collision step, its own collision
	/// detection and serial update are all skipped every frame. An order, damage, speed, a skid or a change of position (a
	/// push found by a moving unit's collision test) wakes it the same frame. Switch: mod option supcommovesleep (default
	/// off). Set by PlanOffTick; read by the other passes.
	bool Asleep(const CUnit* u);
	/// CSupComAirMoveType, landed: true = skip this frame's UpdateLanded (at rest, not the unit's 16th frame)
	bool LandedRest(CUnit* u);
	/// a movement update ran this frame (air move types outside LandedRest)
	void Moved(CUnit* u);
	/// one-line hook in LuaSyncedRead: Spring.SupComHalfRateInfo
	void PushSyncedRead(lua_State* L);
}

#endif
