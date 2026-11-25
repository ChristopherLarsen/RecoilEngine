/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

/* SupCom: a switch that changes the simulation must be the same on every client, so it is a mod option (part of the game setup every
 * client receives), never a per-machine springsetting (landings review 2026-11-24, finding 2: a client with another setting desyncs).
 * Get(name, def): the mod option <name> lowercased ("1"/"0", also "true"/"false") when the game setup has it, else the built-in default.
 * The springsetting of the same name is no longer read. Read lazily, once the game setup exists (the sim's first use). */
#ifndef SUPCOM_SIM_SWITCH_H
#define SUPCOM_SIM_SWITCH_H

#include "Game/GameSetup.h"
#include "System/StringUtil.h"

#include <string>

namespace SupComSimSwitch {
	inline bool Get(const char* name, bool def)
	{
		const auto& mo = CGameSetup::GetModOptions();
		const auto it = mo.find(StringToLower(std::string(name)));
		if (it == mo.end())
			return def;
		const std::string v = StringToLower(it->second);
		if (v == "1" || v == "true")
			return true;
		if (v == "0" || v == "false")
			return false;
		return def;
	}
}

#endif
