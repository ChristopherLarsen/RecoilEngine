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

namespace SupComHalfRate {
	/// one-line hook in GroundMoveSystem before UpdatePreCollisions: true = skip it this frame
	bool SkipGround(CUnit* u);
	/// CSupComAirMoveType, landed: true = skip this frame's UpdateLanded (at rest, not the unit's 16th frame)
	bool LandedRest(CUnit* u);
}

#endif
