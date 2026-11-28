/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 HullTurn: tunables with defaults and source labels (gamedata/kt1/hullturn.lua). Times are seconds, never frames.

#ifndef KT1_HULL_TURN_CONFIG_H
#define KT1_HULL_TURN_CONFIG_H

namespace KT1 {

struct HullTurnConfig {
	float check_s = 0.1f;        // DESIGN 2026-10-28: seconds between checks of a unit that is not turning (3 steps at 30)
	float rest_speed_s = 1.5f;   // DESIGN 2026-10-28: elmos per second below which a unit counts as at rest (0.05 per step at 30)
	float done_rad = 1e-3f;      // DESIGN: radians; a turn this close to the arc edge has arrived
	float axis_rad = 1e-4f;      // DESIGN (engine heading readout): radians kept off an exact axis direction
	int attack_move_cmd = 39404; // DESIGN: the game command id of attack-move (frm_formations), a move order for R7
};

} // namespace KT1

#endif
