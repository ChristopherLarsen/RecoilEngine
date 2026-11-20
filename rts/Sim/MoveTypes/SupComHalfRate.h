/* SupCom: idle units update their movement every other frame (owner decision 2026-11-20, replacing profile cut G).
 * Game key: mod option supcom_half_rate = "1" (absent or other: stock, every frame).
 * A unit is idle when it has no orders, has not been hit for 30 frames, is not carried, built, stunned or skidding, and
 *   ground: stands still with no path, no pending path event and no collision push;
 *   air (CSupComAirMoveType): flies or hovers with no attack target, well above the ground.
 * After 30 idle frames in a row it goes to half rate: units with (id + frame) odd skip that frame's update.
 *   ground: the pre-collision step is skipped (the unit stands still, so nothing is lost);
 *   air: the skipped frame coasts one step at its current velocity, and the next frame turns twice (the doubled dt).
 * Anything that breaks the idle test puts the unit back on every frame at once. All tests read synced state only, so
 * every client makes the same choice. Every 300 frames the share on half rate is logged ([HALFRATE]). */
#ifndef SUPCOM_HALF_RATE_H
#define SUPCOM_HALF_RATE_H

class CUnit;

namespace SupComHalfRate {
	/// one-line hook in GroundMoveSystem before UpdatePreCollisions: true = skip it this frame
	bool SkipGround(CUnit* u);
	/// CSupComAirMoveType::Update: 0 every-frame update, 1 coast this frame, 2 update with a doubled turn
	int Air(CUnit* u, bool idleHere);
}

#endif
