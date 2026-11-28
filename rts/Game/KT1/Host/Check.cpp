/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Host: check-mode results to the log. See Check.h. Check mode only, so it logs at once, not through DisplayEvents.

#include "Check.h"

#include <string>

#include "Game/KT1/Core/Common/Rate.h"
#include "Game/KT1/Core/SimHost/CheckTally.h"
#include "Game/KT1/KT1Registry.h"
#include "System/Log/ILog.h"

namespace KT1 {
namespace {

using Core::SimHost::CheckKey;

// DESIGN: seconds between total lines, so a run that ends without a game over still has a recent total
constexpr double TOTAL_EVERY_S = 10.0;   // KT1 lint: allow double (a Rate argument)

Core::SimHost::CheckTally tally;
Core::SimHost::CheckPairs pairs(tally);
Core::SimHost::WriteGate writeGate;

void Line(const std::string& s) { LOG("%s", s.c_str()); }

std::string FamilyOf(const std::string& system) {
	const Rule* r = Registry::Get().Find(system.c_str());
	return r == nullptr ? std::string() : r->Family();
}

void Describe(const CheckKey& k, const Check::Value* native, const Check::Value& lua) {
	if (native == nullptr) {
		LOG("[KT1CHECK-DIFF] %s %s ids=%d/%d/%d native=none lua=%.9g/%.9g", k.system.c_str(), k.what.c_str(),
			k.id[0], k.id[1], k.id[2], lua.v[0], lua.v[1]);
		return;
	}
	LOG("[KT1CHECK-DIFF] %s %s ids=%d/%d/%d native=%.9g/%.9g lua=%.9g/%.9g", k.system.c_str(), k.what.c_str(),
		k.id[0], k.id[1], k.id[2], native->v[0], native->v[1], lua.v[0], lua.v[1]);
}

} // namespace

bool Check::InCheck(const char* system) {
	const Rule* r = Registry::Get().Find(system);
	return r != nullptr && r->SwitchMode() == Rule::Mode::Check;
}

bool Check::MayWrite() { return writeGate.Allow(tally); }

Core::SimHost::WriteGate& Check::Gate() { return writeGate; }

Check::Running::Running(const char* system, bool check) : Core::SimHost::WriteGate::Scope(writeGate, system, check) {}

void Check::Native(const char* system, const char* what, int id1, int id2, int id3, const Value& value) {
	pairs.Native(CheckKey{system, what, {id1, id2, id3}}, value, Describe);
}

void Check::Native(const char* system, const char* what, int id1, int id2, int id3, float value) {
	Value v;
	v.v[0] = value;
	Native(system, what, id1, id2, id3, v);
}

void Check::Lua(const char* system, const char* what, int id1, int id2, int id3, const Value& value) {
	pairs.Lua(CheckKey{system, what, {id1, id2, id3}}, value, Describe);
}

size_t Check::Mark() { return pairs.Mark(); }

void Check::CloseScope(size_t mark) { pairs.CloseScope(mark); }

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
	pairs.CloseStep(Describe);
	if (tally.Empty()) return;
	tally.CloseStep(frame, Line, FamilyOf);
	if (frame > 0 && frame % Core::Rate::Every(TOTAL_EVERY_S) == 0) Total();
}

// Answers still waiting for the other side are not counted here: the step is not over.
void Check::Total() { tally.Totals(Line, FamilyOf); }

void Check::Reset() {
	pairs.Clear();
	tally.Clear();
}

} // namespace KT1
