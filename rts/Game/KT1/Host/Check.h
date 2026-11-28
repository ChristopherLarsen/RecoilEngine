/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Host: the one way a system reports check-mode results (Core/SimHost/CheckTally.h has the lines).
//
// From C++:   KT1::Check::Float("Orders", "speed", unitID, engine, lua);     one value, bit for bit
//             KT1::Check::Add("Orders", compared, differed, "what differed");
// From Lua:   Spring.KT1.Check.Report("Orders", compared, differed [, "what differed"])
//             Spring.KT1.Check.Total()                                       the total lines now (a test run's end)
// A step with differences logs one line at its end; totals are logged every TOTAL_EVERY_S, at game over, and on Total().

#ifndef KT1_HOST_CHECK_H
#define KT1_HOST_CHECK_H

namespace KT1 {

class Check {
public:
	static void Float(const char* system, const char* what, int id, float engine, float lua);
	static void Add(const char* system, long long compared, long long differed, const char* detail = nullptr);

	static void CloseStep(int frame);   // the step's end (Phases)
	static void Total();                // game over (SimEvents), or a test's end
	static void Reset();                // game shutdown
};

} // namespace KT1

#endif
