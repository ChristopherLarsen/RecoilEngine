/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 SimHost core: the phases of a sim step and the id range a phase function works over. No engine include.
//
// Where each phase sits in CGame::SimFrame (fixed; Host/Phases.h):
//   FrameStart   immediately BEFORE the Lua GameFrame call-in, after the engine's own rules: a ported GameFrame sees the
//                sim state the Lua one saw
//   AfterUnits   after the unit handler's update: movement and weapons are both done
//   FrameEnd     after projectiles, everything else and Lua GameFramePost; then the step is closed
// There is no phase between unit movement and weapon update. It would need a stock hook inside the unit update, agreed
// with the New Engine plan, which will slice there.

#ifndef KT1_PHASE_H
#define KT1_PHASE_H

#include <cstddef>
#include <cstdint>
#include <limits>

namespace KT1::Core::SimHost {

enum class Phase : uint8_t { FrameStart, AfterUnits, FrameEnd, Count };

constexpr size_t PHASES = static_cast<size_t>(Phase::Count);

constexpr const char* PhaseName(Phase p) {
	constexpr const char* names[PHASES] = {"FrameStart", "AfterUnits", "FrameEnd"};
	return (p < Phase::Count) ? names[static_cast<size_t>(p)] : "?";
}

// Half open: [begin, end). A phase function handles the ids inside it and assumes nothing about the rest, so a step can
// later be cut into slices (LUA-FREE-PLAN 8.1 contract 3). The host passes All() today.
struct IdRange {
	int32_t begin = 0;
	int32_t end = std::numeric_limits<int32_t>::max();

	static constexpr IdRange All() { return IdRange{0, std::numeric_limits<int32_t>::max()}; }
	constexpr bool Contains(int32_t id) const { return id >= begin && id < end; }
};

} // namespace KT1::Core::SimHost

#endif
