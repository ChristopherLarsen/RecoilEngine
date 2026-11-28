/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 SimHost core: the phases of a sim step and the id range a phase function works over. No engine include.
//
// Where each phase sits in CGame::SimFrame:
//   FrameStart     before any Lua GameFrame; the events of the time between steps (orders from the net) are in
//   AfterCommands  after the Lua GameFrame, before units move
//   AfterMove      after the unit update (move types, slow update, weapons) and path updates, before projectiles move
//   AfterWeapons   after projectiles, features, unit scripts, LOS and interceptors: this step's hits are applied
//   FrameEnd       after Lua GameFramePost; then the checksum and the display events are closed

#ifndef KT1_PHASE_H
#define KT1_PHASE_H

#include <cstddef>
#include <cstdint>
#include <limits>

namespace KT1::Core::SimHost {

enum class Phase : uint8_t { FrameStart, AfterCommands, AfterMove, AfterWeapons, FrameEnd, Count };

constexpr size_t PHASES = static_cast<size_t>(Phase::Count);

constexpr const char* PhaseName(Phase p) {
	constexpr const char* names[PHASES] = {"FrameStart", "AfterCommands", "AfterMove", "AfterWeapons", "FrameEnd"};
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
