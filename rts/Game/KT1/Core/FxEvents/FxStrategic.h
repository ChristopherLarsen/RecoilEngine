/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 EFFECTS core: the missile relay (replaces the synced half of code/LuaRules/Gadgets/fx_strategic.lua). The silo rules
// (family SPECIAL; while they are Lua, the bridge Spring.KT1.Fx.Silo) report their missiles once a step; the relay turns
// them into the Strategic* , Tactical* and MissileRedirected events (FxEvent.h). Look only. No engine include.

#ifndef KT1_FX_STRATEGIC_H
#define KT1_FX_STRATEGIC_H

#include <cstdint>
#include <span>
#include <vector>

#include "Game/KT1/Core/FxEvents/FxTypes.h"
#include "Game/KT1/Core/FxEvents/FxWorld.h"

namespace KT1::Core::Fx {

struct FxOutput;

struct StrategicConfig {
	float map_unit = 8.0f;             // REQ OUT-0001: elmos per map unit (the damage zone's radius is given in map units)
	template<class S> void Describe(S& s) { s.Field("map_unit", map_unit); }
};

enum class MissileEnd : uint8_t { Flying, Hit, ShotDown, Ended };

// one missile record of the silo rules, this step
struct MissileIn {
	int32_t key = 0;                   // the record's own number: the same every step it lives, never reused
	uint8_t strategic = 0;             // else tactical
	MissileEnd end = MissileEnd::Flying;
	uint8_t redirected = 0;
	int32_t team = 0, allyTeam = 0, owner = 0;
	float sx = 0.0f, sz = 0.0f, x = 0.0f, z = 0.0f;          // launch and target
	uint8_t hasPos = 0;                // tactical: the simulated position is set
	float gx = 0.0f, gz = 0.0f;
	int32_t orderStep = 0, launchStep = 0, hitStep = 0, interceptStep = 0, endStep = 0;
	int32_t defenceUnit = 0;           // the defence that will intercept it; 0: none
	float defenceX = 0.0f, defenceZ = 0.0f;
	int32_t defenceAlly = -1;
	float outerRadiusUnits = 0.0f;     // the outer damage zone
	int32_t outerSteps = 1;
	float outerSpreadS = 0.0f;
	uint8_t hasShot = 0;               // tactical: a defence shot is after it
	float shotX = 0.0f, shotZ = 0.0f, shotFromX = 0.0f, shotFromZ = 0.0f;
	uint8_t hasInterceptPos = 0;
	float interceptX = 0.0f, interceptZ = 0.0f;
};

struct InterceptorIn {
	int32_t id = 0;
	int32_t targetKey = 0;             // MissileIn::key
	int32_t allyTeam = -1;
	float sx = 0.0f, sz = 0.0f, x = 0.0f, z = 0.0f;
	int32_t launchStep = 0, lastStep = 0;
	MissileEnd end = MissileEnd::Flying;
};

struct RedirectIn {                    // one redirection, reported once
	int32_t missileKey = 0, redirectorUnit = 0;
	float x = 0.0f, z = 0.0f, fromX = 0.0f, fromZ = 0.0f, toX = 0.0f, toZ = 0.0f;
};

struct TrackedMissile {
	int32_t key = 0, id = 0;           // id: the relay's number, as sent
	uint8_t strategic = 0, launched = 0, turned = 0;
	float x = 0.0f, z = 0.0f, gx = 0.0f, gz = 0.0f;
	float progress = 0.0f, pathLength = 0.0f;
	float turnProgress = 0.0f, turnLength = 0.0f;
};

struct StrategicState {
	std::vector<TrackedMissile> tracked;       // in the order first seen
	std::vector<int32_t> interceptors;         // announced and not ended, by id
	int32_t nextId = 0;
};

void RelayStrategic(StrategicState& s, const StrategicConfig& c, std::span<const MissileIn> missiles,
	std::span<const InterceptorIn> interceptors, std::span<const RedirectIn> redirects, int32_t step, const WorldQuery& world,
	FxOutput& out);

} // namespace KT1::Core::Fx

#endif
