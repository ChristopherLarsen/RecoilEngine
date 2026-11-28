/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Core/Common: Rate - converts seconds to sim steps from the one sim rate.
// Why: no KT1 system holds a frame count (LUA-FREE-PLAN §2.2 p6, SIM-10HZ-PLAN stage 0). Config gives periods and delays
//   in seconds (`period_s`); a system converts them once, at Start, with Rate::Every / Rate::Steps.
// The rate is set once per game, before any system starts, by the host from the engine's GAME_SPEED
//   (Rate::SetStepsPerSecond). It is the same on every client: SIM-10HZ stage 0 puts the rate in the sync check.
// There is no default: a conversion before SetStepsPerSecond asserts in dev, so no system can run at a guessed rate.
//   In release it degrades to "every step" (Every 1, Steps 0, Seconds 0).
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
		if (stepsPerSecond >= 1)
			sps = stepsPerSecond;
	}
	static bool IsSet() { return sps >= 1; }
	static int32_t StepsPerSecond() { assert(IsSet()); return sps; }
	static double StepSeconds() { assert(IsSet()); return IsSet() ? static_cast<double>(1) / sps : 0; }

	// a period: steps between two visits, at least 1 (something "every 0 s" runs every step)
	static int32_t Every(double seconds) { return std::max<int32_t>(1, Round(seconds)); }

	// a delay: steps until something happens, at least 0 (0 s is "this step")
	static int32_t Steps(double seconds) { return std::max<int32_t>(0, Round(seconds)); }

	// steps back to seconds, for logs and for config written back out
	static double Seconds(int32_t steps) { assert(IsSet()); return IsSet() ? static_cast<double>(steps) / sps : 0; }

private:
	static int32_t Round(double seconds) {
		assert(IsSet());
		return static_cast<int32_t>(std::lround(seconds * sps));
	}

	inline static int32_t sps = 0;   // 0: not set yet
};

} // namespace KT1::Core

#endif
