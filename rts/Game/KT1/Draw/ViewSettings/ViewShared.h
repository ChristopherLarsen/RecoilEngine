/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1: what the two view systems share. "Zoom" (always on) reads the camera and keeps the zoom level and the gates for
// every reader (ZoomGatesApi.h, Spring.KT1.Zoom). "ViewSettings" (switched) acts on them: engine settings and draw masks.
// Unsynced: draw only.

#ifndef KT1_DRAW_VIEW_SHARED_H
#define KT1_DRAW_VIEW_SHARED_H

#include "Game/KT1/Core/ViewSettings/ViewCullState.h"
#include "Game/KT1/Core/ViewSettings/ViewSceneState.h"
#include "Game/KT1/Core/ViewSettings/ViewSettingsState.h"

namespace KT1::ViewSet {

struct ViewShared {
	Core::View::ViewSettingsConfig config;   // data/kt1/viewsettings.json
	bool loaded = false;                     // the config was read this game
	bool started = false;                    // Zoom has compiled the table
	Core::View::ViewState view;
	Core::View::ShadowState shadow;
	Core::View::SceneState scene;
	bool engineFog = false;                  // the vision overlay draws with the engine fog mask
};

ViewShared& Shared();

} // namespace KT1::ViewSet

#endif
