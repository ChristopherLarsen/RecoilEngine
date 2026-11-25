/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#include "SupComPushCap.h"

#include "Game/GameSetup.h"
#include "System/SupComSimSwitch.h"
#include "System/StringUtil.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>

namespace {
	// a numeric mod option (the game setup, the same on every client), else the default; parsed once
	float Opt(const char* name, float def)
	{
		const auto& mo = CGameSetup::GetModOptions();
		const auto it = mo.find(StringToLower(std::string(name)));
		if (it == mo.end())
			return def;
		char* end = nullptr;
		const float v = std::strtof(it->second.c_str(), &end);
		return (end != it->second.c_str() && std::isfinite(v) && v >= 0.0f)? v: def;
	}

	// PLACEHOLDER (pending the Reference pathing release): step plus push per frame, as a fraction of top speed
	float TotalFrac() { static const float v = Opt("supcompushcap_total", 1.0f); return v; }
	// PLACEHOLDER (pending the Reference pathing release): the push always allowed, as a fraction of top speed
	float FloorFrac() { static const float v = Opt("supcompushcap_floor", 0.25f); return v; }
}

bool SupComPushCap::On()
{
	static const bool on = SupComSimSwitch::Get("SupComPushCap", true);
	return on;
}

void SupComPushCap::Clamp(float3& push, const float3& step, float maxSpeed)
{
	if (!On() || maxSpeed <= 0.0f)
		return;

	const float stepLen = std::sqrt(step.x * step.x + step.z * step.z);
	const float limit = std::max(TotalFrac() * maxSpeed - stepLen, FloorFrac() * maxSpeed);
	const float len = std::sqrt(push.x * push.x + push.z * push.z);

	if (len <= limit || len <= 0.0f)
		return;

	const float k = limit / len;
	push.x *= k;
	push.z *= k;
}

float SupComPushCap::PushClearFrac() { static const float v = Opt("supcompushclear", 0.1f); return v; }
float SupComPushCap::PushBackFrac() { static const float v = Opt("supcompushback", 0.25f); return v; }
bool SupComPushCap::NoSlide() { static const bool on = SupComSimSwitch::Get("SupComNoSlide", true); return on; }
