/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 EFFECTS core: when a death's blasts happen (replaces the timing half of gamedata/fx_criteria.lua). The synced system
// hides a unit wreck until the main blast and the draw side (Draw/FxBuild) draws the chain; both roll the same numbers
// from the dying unit's id. Radii in map units (U), times in seconds. No engine include.

#ifndef KT1_FX_CRITERIA_H
#define KT1_FX_CRITERIA_H

#include <cstdint>

#include "Game/KT1/Core/FxEvents/FxTypes.h"

namespace KT1::Core::Fx {

struct CriteriaTimingConfig {
	float map_unit = 8.0f;              // DESIGN: elmos per U (FX-PIPELINE.md)
	float air_above = 24.0f;            // DESIGN: higher above ground or water than this is an air death, elmos
	float start_delay_s = 0.2f;         // PARITY: measured, a land death's effects start this long after the kill
	float start_delay_random_s = 0.1f;  // PARITY: plus up to this (none in the air)
	float chain_min_radius = 1.0f;      // DESIGN: smaller structures have no chain, U
	float chain_span_max_s = 2.0f;      // DESIGN: the longest span from the arcs to the main blast
	float chain_span_per_radius_s = 0.4f;     // DESIGN: span per U of footprint radius
	float chain_count_per_radius = 0.75f;     // DESIGN: small blasts per U, times 1 to 2 at random
	int chain_count_max = 8;            // DESIGN
	float chain_from = 0.3f;            // DESIGN: the first small blast, as a share of the span
	float chain_over = 0.55f;           // DESIGN: and the last this much later
	float chain_jitter_s = 0.05f;       // DESIGN
	float anim_gap_s = 0.1f;            // DESIGN: between blasts of an animated death, plus up to as much again
	float anim_tail_s = 0.3f;           // DESIGN: main blast after the last of them
	// the synced half only: hiding the wreck
	float hide_reach_min = 32.0f;       // DESIGN: a wreck this near the death is that death's, elmos
	float hide_reach_per_radius = 1.5f; // DESIGN: or within this many footprint radii
	float hide_after_s = 0.067f;        // DESIGN: the wreck shows this long after the main blast (2 frames)
	float hide_wait_s = 2.0f;           // DESIGN: a death still without a wreck after this is dropped
	template<class S> void Describe(S& s) {
		s.Field("map_unit", map_unit);
		s.Field("air_above", air_above);
		s.Field("start_delay_s", start_delay_s);
		s.Field("start_delay_random_s", start_delay_random_s);
		s.Field("chain_min_radius", chain_min_radius);
		s.Field("chain_span_max_s", chain_span_max_s);
		s.Field("chain_span_per_radius_s", chain_span_per_radius_s);
		s.Field("chain_count_per_radius", chain_count_per_radius);
		s.Field("chain_count_max", chain_count_max);
		s.Field("chain_from", chain_from);
		s.Field("chain_over", chain_over);
		s.Field("chain_jitter_s", chain_jitter_s);
		s.Field("anim_gap_s", anim_gap_s);
		s.Field("anim_tail_s", anim_tail_s);
		s.Field("hide_reach_min", hide_reach_min);
		s.Field("hide_reach_per_radius", hide_reach_per_radius);
		s.Field("hide_after_s", hide_after_s);
		s.Field("hide_wait_s", hide_wait_s);
	}
};

// the small seeded generator of fx_criteria.lua C.Rng (32 bit)
struct CriteriaRng {
	uint32_t state = 0;
	explicit CriteriaRng(int32_t seed);
	float Next();                       // [0, 1)
};

constexpr int32_t CHAIN_MAX = 32;       // DESIGN: blast times one chain holds

struct DeathChain {
	float delayS = 0.0f;                // kill to the effects' start
	float mainS = 0.0f;                 // effects' start to the main blast
	int32_t count = 0;                  // small blasts before it
	float timesS[CHAIN_MAX] = {};       // from the effects' start
};

// radius in U; animLengthS 0: no death animation. Rolls the generator in the Lua's order (delay, then the chain).
DeathChain PlanDeathChain(const CriteriaTimingConfig& c, float radius, DeathMedium medium, bool structure, float animLengthS,
	CriteriaRng& rng);
float MainBlastAfterS(const CriteriaTimingConfig& c, float radius, DeathMedium medium, bool structure, CriteriaRng& rng);
DeathMedium MediumOfDeath(const CriteriaTimingConfig& c, float y, float groundHeight);

} // namespace KT1::Core::Fx

#endif
