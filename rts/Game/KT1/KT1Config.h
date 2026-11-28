/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 rule config helpers for the sim rate (KT1 SIM-10HZ-PLAN stage 0): a rule config gives times in seconds and speeds
// per frame at 30 Hz, and the rule reads them in frames at GAME_SPEED. Every helper is the identity at 30 Hz.
#pragma once

#include <algorithm>
#include <cmath>
#include <string>

#include "Lua/LuaParser.h"
#include "Sim/Misc/GlobalConstants.h"

namespace KT1 {

// A cadence or wait: keyS in seconds wins; else keyF, frames counted at 30 Hz (the files of older pins). Whole sim frames
// at GAME_SPEED, rounded, at least 1.
inline int ConfigFrames(const LuaTable& cfg, const std::string& keyS, const std::string& keyF, int defFrames30)
{
	if (cfg.KeyExists(keyS))
		return std::max(1, int(std::floor(double(cfg.GetFloat(keyS, 0.0f)) * GAME_SPEED + 0.5)));

	const int f30 = cfg.GetInt(keyF, defFrames30);
	return std::max(1, (GAME_SPEED == 30)? f30: int(std::floor(double(f30) * GAME_SPEED / 30.0 + 0.5)));
}

// A speed threshold given in elmos per frame at 30 Hz, per sim frame at GAME_SPEED (unit speeds per frame scale with the
// frame length).
inline float ConfigPerFrame30(const LuaTable& cfg, const std::string& key, float def30)
{
	const float v = cfg.GetFloat(key, def30);
	return (GAME_SPEED == 30)? v: v * (30.0f / GAME_SPEED);
}

} // namespace KT1
