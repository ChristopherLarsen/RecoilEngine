/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 EFFECTS core: death blast timing (gamedata/fx_criteria.lua C.Rng, C.StartDelay, C.Chain, C.MainAt and the medium
// rule of gfx_criteria_fx.lua). The Lua number is float, so the Lua's "32 bit" generator runs in float: its state is a
// whole number under 2^32 that a float holds exactly. No engine include.

#include "Game/KT1/Core/FxEvents/FxCriteria.h"

#include <algorithm>
#include <cmath>

#include "Game/KT1/Core/Common/DetMath.h"

namespace KT1::Core::Fx {

namespace {

constexpr float SEED_FACTOR = 2654435761.0f;     // DESIGN: fx_criteria.lua C.Rng
constexpr float SEED_ADD = 12345.0f;             // DESIGN: fx_criteria.lua C.Rng
constexpr float STEP_FACTOR = 1664525.0f;        // DESIGN: fx_criteria.lua C.Rng
constexpr float STEP_ADD = 1013904223.0f;        // DESIGN: fx_criteria.lua C.Rng
constexpr float RANGE = 4294967296.0f;           // DESIGN: 2^32
constexpr float ANIM_SLACK_S = 0.000001f;        // DESIGN: fx_criteria.lua C.Chain, the animation's end

// the blasts of an animated death: one after another until the animation ends
void AnimatedChain(const CriteriaTimingConfig& c, float animLengthS, CriteriaRng& rng, DeathChain& chain) {
	float t = 0, last = 0;
	while (t < animLengthS - ANIM_SLACK_S) {
		if (chain.count < CHAIN_MAX) chain.timesS[chain.count++] = t;
		last = t;
		t = t + c.anim_gap_s + c.anim_gap_s * rng.Next();
	}
	chain.mainS = last + c.anim_tail_s;
}

// the small blasts over a structure's body, spread over a span that grows with its radius
void SpreadChain(const CriteriaTimingConfig& c, float radius, CriteriaRng& rng, DeathChain& chain) {
	const float span = std::min(c.chain_span_max_s, c.chain_span_per_radius_s * radius);
	const float wanted = std::floor(c.chain_count_per_radius * radius * (1 + rng.Next()));
	const int32_t most = std::min<int32_t>(c.chain_count_max, CHAIN_MAX);
	const int32_t n = std::max<int32_t>(1, std::min<int32_t>(most, static_cast<int32_t>(wanted)));
	for (int32_t i = 0; i < n; ++i) {
		const float k = n > 1 ? static_cast<float>(i) / static_cast<float>(n - 1)
		                      : 0.5f;                  // DESIGN: a single blast sits mid-span
		chain.timesS[i] = span * (c.chain_from + c.chain_over * k) + c.chain_jitter_s * rng.Next();
	}
	chain.count = n;
	chain.mainS = span;
}

} // namespace

CriteriaRng::CriteriaRng(int32_t seed) {
	const float s = static_cast<float>(seed) * SEED_FACTOR + SEED_ADD;
	state = static_cast<uint32_t>(DetMath::Mod(s, RANGE));
}

float CriteriaRng::Next() {
	const float s = static_cast<float>(state) * STEP_FACTOR + STEP_ADD;
	const float next = DetMath::Mod(s, RANGE);
	state = static_cast<uint32_t>(next);
	return next / RANGE;
}

DeathChain PlanDeathChain(const CriteriaTimingConfig& c, float radius, DeathMedium medium, bool structure, float animLengthS,
	CriteriaRng& rng) {
	DeathChain chain;
	const bool air = medium == DeathMedium::Air;
	chain.delayS = air ? 0 : c.start_delay_s + c.start_delay_random_s * rng.Next();
	if (!structure || air || radius < c.chain_min_radius) return chain;
	if (animLengthS > 0) AnimatedChain(c, animLengthS, rng, chain);
	else SpreadChain(c, radius, rng, chain);
	return chain;
}

float MainBlastAfterS(const CriteriaTimingConfig& c, float radius, DeathMedium medium, bool structure, CriteriaRng& rng) {
	const DeathChain chain = PlanDeathChain(c, radius, medium, structure, 0, rng);
	return chain.delayS + chain.mainS;
}

DeathMedium MediumOfDeath(const CriteriaTimingConfig& c, float y, float groundHeight) {
	if (y - std::max(groundHeight, 0.0f) > c.air_above) return DeathMedium::Air;
	return groundHeight < 0 ? DeathMedium::Water : DeathMedium::Land;
}

} // namespace KT1::Core::Fx
