/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Core/Common: Rate - converts seconds to sim steps from the one sim rate.
// Why: no KT1 system holds a frame count (LUA-FREE-PLAN §2.2 p6, SIM-10HZ-PLAN stage 0). Config gives periods and delays
//   in seconds (`period_s`); a system converts them once, at Start, with Rate::Every / Rate::Steps.
// The rate is set once per game, before any system starts, by the host from the engine's GAME_SPEED
//   (Rate::SetStepsPerSecond). It is the same on every client: SIM-10HZ stage 0 puts the rate in the sync check.
// Rounding is std::lround (half away from zero), so 0.5 s is 15 steps at 30 and 5 at 10; 0.25 s at 10 is 3 steps.
// No engine include, no allocation.

#ifndef KT1_CORE_COMMON_RATE_H
#define KT1_CORE_COMMON_RATE_H

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>

namespace KT1::Core {

class Rate {
public:
	// the host calls this once per game (tests call it per pass); a rate under 1 is refused
	static void SetStepsPerSecond(int32_t stepsPerSecond) {
		assert(stepsPerSecond >= 1);
		sps = std::max<int32_t>(1, stepsPerSecond);
	}
	static int32_t StepsPerSecond() { return sps; }
	static double StepSeconds() { return 1.0 / sps; }

	// a period: steps between two visits, at least 1 (something "every 0 s" runs every step)
	static int32_t Every(double seconds) { return std::max<int32_t>(1, Round(seconds)); }

	// a delay: steps until something happens, at least 0 (0 s is "this step")
	static int32_t Steps(double seconds) { return std::max<int32_t>(0, Round(seconds)); }

	// steps back to seconds, for logs and for config written back out
	static double Seconds(int32_t steps) { return static_cast<double>(steps) / sps; }

private:
	static int32_t Round(double seconds) { return static_cast<int32_t>(std::lround(seconds * sps)); }

	// 30 until the host sets it: the engine's rate before SIM-10HZ stage 1
	inline static int32_t sps = 30;
};

} // namespace KT1::Core

#endif
