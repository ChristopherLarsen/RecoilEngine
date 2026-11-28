/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Host: the one way a system reports check-mode results (Core/SimHost/CheckTally.h has the lines,
// Core/SimHost/CheckPairs.h the pairing).
//
// A value both sides compute, matched by key (system, what, three ids; unused ids 0), in either order:
//   C++   KT1::Check::Native("Orders", "speed", unitID, 0, 0, value);
//   Lua   Spring.KT1.Check.Answer("Orders", "speed", unitID, 0, 0, answer [, answer2 [, input]])
// An Allow* answer is given by AllowChain for the native side; the replaced gadget calls Answer from inside its handler
// (AllowChain.h has the keys).
// Compared elsewhere:
//   C++   KT1::Check::Float("Orders", "speed", unitID, engine, lua);     one value, bit for bit
//         KT1::Check::Add("Orders", compared, differed, "what differed");
//   Lua   Spring.KT1.Check.Report("Orders", compared, differed [, "what differed"])
//         Spring.KT1.Check.Total()                                       the total lines now (a test run's end)
// A step with differences logs one line at its end; cumulative totals are logged every TOTAL_EVERY_S, at game over, and
// on Total(). The last total line of a system is its result.

#ifndef KT1_HOST_CHECK_H
#define KT1_HOST_CHECK_H

#include <cstddef>

#include "Game/KT1/Core/SimHost/CheckPairs.h"

namespace KT1 {

class Check {
public:
	using Value = Core::SimHost::CheckValue;

	static void Native(const char* system, const char* what, int id1, int id2, int id3, const Value& value);
	static void Native(const char* system, const char* what, int id1, int id2, int id3, float value);
	static void Lua(const char* system, const char* what, int id1, int id2, int id3, const Value& value);

	// A call's scope: native answers given inside it that Lua does not match are unmatched when it closes.
	static size_t Mark();
	static void CloseScope(size_t mark);

	static void Float(const char* system, const char* what, int id, float engine, float lua);
	static void Add(const char* system, long long compared, long long differed, const char* detail = nullptr);

	static void CloseStep(int frame);   // the step's end (Phases)
	static void Total();                // game over (SimEvents), or a test's end
	static void Reset();                // game shutdown
};

} // namespace KT1

#endif
