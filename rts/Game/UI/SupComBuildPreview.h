/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#ifndef SUPCOM_BUILD_PREVIEW_H
#define SUPCOM_BUILD_PREVIEW_H

struct lua_State;

// supcom: springsetting SupComBuildPreviewHide (default 1; 0 ignores the call, as stock). The game's own placement
// interface can hide the engine's build preview (unsynced Lua Spring.SetEngineBuildPreview(false)): the cursor ghost,
// its build square and ranges, and the Shift-drag line of previews are not drawn, so the game draws every site itself.
// Only the drawing: where a click would build is unchanged (the game issues its own build orders).
namespace SupComBuildPreview {
	/// the engine's build preview is hidden (Lua asked and the setting allows it)
	bool Hidden();

	/// LuaUnsyncedCtrl: SetEngineBuildPreview
	void PushUnsyncedCtrl(lua_State* L);
}

#endif
