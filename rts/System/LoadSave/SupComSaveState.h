/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#ifndef SUPCOM_SAVE_STATE_H
#define SUPCOM_SAVE_STATE_H

// supcom: springsetting SaveGameFullState (default 1; 0 is stock). On, a save game also keeps the synced random
// generator, the HAPFS path state (SupComPathState.cpp), the smooth height mesh and the craters still being dug
// (SupComTerrain.cpp); a loaded game rebuilds its LOS maps before the first frame, ground move types keep their path ids,
// and a demo recorded from a loaded game names the save and starts from it. So a loaded game goes on exactly as the saved
// one. Off, none of this is written or read (stock saves, stock load). Saves made with one setting load only with the same.
// Not switched: projectiles' createMe flag is always saved (a creg member; a loaded projectile is not announced again).
namespace SupComSaveState {
	bool FullState();
}

#endif
