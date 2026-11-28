/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 EFFECTS core: who is building what (replaces the Task, InReach, NanoPiece and construction loop rules of
// gamedata/fx_build_logic.lua). Rules/FxEvents uses them to publish BuildSample (FxSamples.h); the sound side uses the
// loop rules for the construction loops. No engine include.

#ifndef KT1_FX_BUILD_WORK_H
#define KT1_FX_BUILD_WORK_H

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "Game/KT1/Core/FxEvents/FxSamples.h"

namespace KT1::Core::Fx {

struct BuildWorkConfig {
	float range_slack = 24.0f;        // DESIGN: elmos beyond build distance plus target radius that still count as at work
	std::string nano_names = "nano+emit+beam+flare";   // DESIGN: piece name parts that mark the nano source, best first
	int hear_max = 4;                 // DESIGN: owner 2026-10-30: construction loops heard at once
	float loop_min_s = 0.5f;          // DESIGN: the shortest gap between passes
	float loop_overlap = 0.95f;       // DESIGN: the next pass starts at this share of the last
	float loop_check_s = 0.333f;      // DESIGN: between the beam-on checks
	template<class S> void Describe(S& s) {
		s.Field("range_slack", range_slack);
		s.Field("nano_names", nano_names);
		s.Field("hear_max", hear_max);
		s.Field("loop_min_s", loop_min_s);
		s.Field("loop_overlap", loop_overlap);
		s.Field("loop_check_s", loop_check_s);
	}
};

// what a builder is told to do, as the engine knows it
struct BuilderOrder {
	int32_t building = -1;            // the unit it is building or repairing right now; -1: none
	int32_t command = 0;              // its current command; 0: none
	int32_t parameter = -1;           // the command's first parameter
	int32_t parameters = 0;           // how many it has (an area order has more than one: no single target)
};

struct BuildTarget { bool working = false; BuildWork kind = BuildWork::Build; int32_t target = -1; bool feature = false; };

// maxUnits: reclaim targets at or above it are features
BuildTarget TargetOf(const BuilderOrder& order, int32_t maxUnits);
bool InReach(const BuildWorkConfig& c, float distance, float buildDistance, float targetRadius);
// the index of the nano piece among the piece names; -1: none
int32_t NanoPiece(const BuildWorkConfig& c, std::span<const std::string_view> pieces);

struct WorkingBuilder { int32_t unit = -1; float distance = 0.0f; };
struct LoopState { int32_t unit = -1; float due = 0.0f; };

// the nearest hear_max working builders whose loop is due now; builders no longer heard are forgotten
void LoopsDue(const BuildWorkConfig& c, std::vector<LoopState>& state, std::span<const WorkingBuilder> working, float now,
	std::vector<int32_t>& due);
void LoopNext(const BuildWorkConfig& c, std::vector<LoopState>& state, int32_t unit, float now, float lengthS);

} // namespace KT1::Core::Fx

#endif
