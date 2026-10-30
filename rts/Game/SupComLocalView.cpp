/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#include "SupComLocalView.h"

#include "Game/GameSetup.h"
#include "Game/GlobalUnsynced.h"
#include "Lua/LuaUI.h"
#include "System/Config/ConfigHandler.h"
#include "System/Log/ILog.h"

CONFIG(bool, SupComLocalFullView).defaultValue(true).description("supcom: in a game with fog of war off (mod option fog), a player's own client draws the whole map without changing what the player's army sees; 0 draws as stock Recoil.");

namespace {
	int active = -1; // -1: not decided yet (the game setup and the config are read once)
}

static bool FogOff()
{
	const auto& opts = CGameSetup::GetModOptions();
	const auto it = opts.find("fog");

	if (it == opts.end())
		return false;

	return (it->second != "1" && it->second != "true");
}

bool SupComLocalView::Active()
{
	if (gu == nullptr || gu->spectating)
		return false;

	if (active < 0) {
		const bool setting = configHandler->GetBool("SupComLocalFullView");
		const bool fogOff = FogOff();
		active = (setting && fogOff)? 1: 0;
		LOG("[SupComLocalView] local full view %s (SupComLocalFullView %d, fog of war off %d)", (active != 0)? "on": "off", setting, fogOff);
	}

	return (active != 0);
}

void SupComLocalView::Apply()
{
	if (!Active())
		return; // spectators keep the stock /specfullview; players keep stock LOS

	gu->spectatingFullView = true;

	if (luaUI != nullptr)
		CLuaUI::UpdateTeams();
}

bool SupComLocalView::OrderFullView()
{
	return (gu->spectatingFullView && !Active());
}
