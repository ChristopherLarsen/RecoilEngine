/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 EFFECTS draw: the family's one seeded random generator and its clock. Every effect draws its numbers from a
// generator seeded by what it is about, so a capture of the same scene gives the same picture (no engine random, no wall
// clock). Draw side only.

#ifndef KT1_FX_RANDOM_H
#define KT1_FX_RANDOM_H

#include <cstdint>

namespace KT1 {

class FrameFacts;

namespace FxDraw {

// the minimal standard generator the Lua effects seed (q = q * 16807 mod 2147483647)
class Random {
public:
	explicit Random(uint32_t seed = 1) { Seed(seed); }
	void Seed(uint32_t seed);
	// a seed made of two or three numbers (an id and a step, a cell and a type)
	void SeedFrom(uint32_t a, uint32_t b, uint32_t c = 0);
	float Unit();                                  // (0, 1)
	float Range(float lo, float hi);
	float Signed();                                // (-1, 1)
	int32_t Int(int32_t lo, int32_t hi);           // both ends

private:
	uint32_t state = 1;
};

// The frame's effect time: the sim's, blended into the step, so it stops with the game.
struct FxTime {
	float now = 0.0f;           // seconds of game time at this drawn frame
	float delta = 0.0f;         // seconds since the frame `previous` was made of; 0 when the game stands
	float simNow = 0.0f;        // seconds of game time at the current step
	int32_t step = 0;
	uint32_t frame = 0;         // the drawn frame
};

// previous: the last frame's FxTime (default: the first frame)
FxTime TimeOf(const FrameFacts& facts, const FxTime& previous);

} // namespace FxDraw
} // namespace KT1

#endif
