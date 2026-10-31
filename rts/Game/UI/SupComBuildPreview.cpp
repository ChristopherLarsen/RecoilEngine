/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#include "SupComBuildPreview.h"

#include "Lua/LuaUtils.h"
#include "System/Config/ConfigHandler.h"

CONFIG(bool, SupComBuildPreviewHide).defaultValue(true).description("supcom: the game may hide the engine's build preview (Spring.SetEngineBuildPreview(false)) and draw placement itself; 0 ignores the call, as stock Recoil.");

namespace {
	bool hidden = false;
	int setting = -1; // -1: not read yet
}

bool SupComBuildPreview::Hidden()
{
	if (!hidden)
		return false;

	if (setting < 0)
		setting = configHandler->GetBool("SupComBuildPreviewHide")? 1: 0;

	return (setting != 0);
}

static int SetEngineBuildPreview(lua_State* L)
{
	hidden = !luaL_checkboolean(L, 1);
	return 0;
}

void SupComBuildPreview::PushUnsyncedCtrl(lua_State* L)
{
	LuaPushRawNamedCFunc(L, "SetEngineBuildPreview", SetEngineBuildPreview);
}
