/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#include "SupComSaveState.h"

#include "System/Config/ConfigHandler.h"

CONFIG(bool, SaveGameFullState).defaultValue(true).description("supcom: save games also keep the random generator, path state, smooth mesh and running craters, so a loaded game goes on exactly as the saved one (1); stock saves (0). Saves load only with the setting they were made with.");

namespace SupComSaveState {
	bool FullState()
	{
		return (configHandler == nullptr || configHandler->GetBool("SaveGameFullState"));
	}
}
