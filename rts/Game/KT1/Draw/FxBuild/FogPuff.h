/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 FxBuild: the faint puff where a unit died out of the viewer's sight (replaces the draw half of
// LuaRules/Gadgets/gfx_fog_death_puff.lua), fed by FogDeath: shown only without a full view, to another ally team,
// when the point was not in sight. Engine-free.

#ifndef KT1_FOG_PUFF_H
#define KT1_FOG_PUFF_H

#include <cstdint>
#include <vector>

#include "Game/KT1/Core/FxEvents/FxEvent.h"

namespace KT1::FxBuild {

struct FogPuffConfig {                // "fog_puff"
	float rise = 0.30f;               // PLACEHOLDER
	float life = 0.3f;                // PLACEHOLDER: seconds
	float size = 2.0f;                // PLACEHOLDER: times the radius
	float min_radius = 12.0f;         // PLACEHOLDER
	int segments = 20;                // PLACEHOLDER
	float lift = 2.0f;                // PLACEHOLDER: elmos above the ground plus the unit's height
	template<class S> void Describe(S& s) {
		s.Field("rise", rise);
		s.Field("life", life);
		s.Field("size", size);
		s.Field("min_radius", min_radius);
		s.Field("segments", segments);
		s.Field("lift", lift);
	}
};

struct Puff { Core::Fx::Vec3 pos; float radius = 0.0f, born = 0.0f; };
struct FogPuffState { std::vector<Puff> puffs; uint32_t shown = 0; };

// false: the viewer saw it (or a full view): nothing is drawn
bool OnFogDeath(FogPuffState& s, const FogPuffConfig& c, const Core::Fx::FogDeath& e, bool seen, bool allied,
	bool fullView, float groundHeight, float now);
void ExpirePuffs(FogPuffState& s, const FogPuffConfig& c, float now);

} // namespace KT1::FxBuild

#endif
