/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Host: check-mode results to the log. See Check.h. Check mode only, so it logs at once, not through DisplayEvents.

#include "Check.h"

#include <string>

#include "Game/KT1/Core/Common/Rate.h"
#include "Game/KT1/Core/SimHost/CheckTally.h"
#include "System/Log/ILog.h"

namespace KT1 {
namespace {

// DESIGN: seconds between total lines, so a run that ends without a game over still has a recent total
constexpr double TOTAL_EVERY_S = 10.0;   // KT1 lint: allow double (a Rate argument)

Core::SimHost::CheckTally tally;

void Line(const std::string& s) { LOG("%s", s.c_str()); }

} // namespace

void Check::Float(const char* system, const char* what, int id, float engine, float lua) {
	const bool same = Core::SimHost::CheckTally::SameBits(engine, lua);
	if (tally.Add(system, 1, same ? 0 : 1))
		LOG("[KT1CHECK-DIFF] %s %s id=%d engine=%.9g lua=%.9g", system, what, id, engine, lua);
}

void Check::Add(const char* system, long long compared, long long differed, const char* detail) {
	if (tally.Add(system, compared, differed) && detail != nullptr)
		LOG("[KT1CHECK-DIFF] %s %s", system, detail);
}

void Check::CloseStep(int frame) {
	if (tally.Empty()) return;
	tally.CloseStep(frame, Line);
	if (frame > 0 && frame % Core::Rate::Every(TOTAL_EVERY_S) == 0) Total();
}

void Check::Total() { tally.Totals(Line); }

void Check::Reset() { tally.Clear(); }

} // namespace KT1
