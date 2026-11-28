/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Core/IconLexicon tests: IconOrder, the instance slots in draw order (LUA-FREE-WORKPACKAGES D01).
// Port of icon_lex.lua's incremental rules (IncApply, IncGone, GpuRebuild) and tools/test/cases/ico_contacts_order.lua.

#include "../KT1Test.h"
#include "Game/KT1/Core/IconLexicon/IconOrder.h"

#include <vector>

using namespace KT1::Core::IconLexicon;
using Entry = IconOrder::Entry;
using Action = IconOrder::Action;

namespace {
	IconOrder Made(const std::vector<Entry>& units, int maxSlots = 64) {
		IconOrder o;
		o.Init(100, maxSlots);
		o.Rebuild(units);
		return o;
	}
}

KT1_CASE("ICOLEX-ORDER-BORN", "CAM-003 v5 R10: draw order is creation order, then unit id; later-created on top") {
	IconOrder o = Made({{7, 3, Kind::Full}, {2, 1, Kind::Contact}, {9, 1, Kind::Full}, {4, 2, Kind::Full}, {5, 0, Kind::None}});
	KT1_CHECK_EQ(o.Count(), 4);
	KT1_CHECK_EQ(o.IdAt(0), 2);
	KT1_CHECK_EQ(o.IdAt(1), 9);
	KT1_CHECK_EQ(o.IdAt(2), 4);
	KT1_CHECK_EQ(o.IdAt(3), 7);
	KT1_CHECK_EQ(o.SlotOf(5), IconOrder::NONE);
}

KT1_CASE("ICOLEX-ORDER-APPEND", "the newest unit is appended at the end; an older one needs a rebuild") {
	IconOrder o = Made({{1, 1, Kind::Full}, {2, 2, Kind::Full}});
	const auto c = o.Update(3, 3, Kind::Full);
	KT1_CHECK(c.action == Action::Append);
	KT1_CHECK_EQ(c.slot, 2);
	KT1_CHECK_EQ(o.Count(), 3);
	KT1_CHECK(o.Update(4, 2, Kind::Full).action == Action::Rebuild);
	KT1_CHECK(o.Update(5, 9, Kind::None).action == Action::None);
}

KT1_CASE("ICOLEX-ORDER-INPLACE", "a kind change is written in the unit's own slot; full is always rewritten; an unchanged contact sends nothing") {
	IconOrder o = Made({{1, 1, Kind::Full}, {2, 2, Kind::Contact}});
	auto c = o.Update(2, 2, Kind::Full);
	KT1_CHECK(c.action == Action::Write && c.slot == 1);
	KT1_CHECK(o.KindAt(1) == Kind::Full);
	KT1_CHECK(o.Update(1, 1, Kind::Full).action == Action::Write);
	KT1_CHECK(o.Update(2, 2, Kind::Contact).action == Action::Write);
	KT1_CHECK(o.Update(2, 2, Kind::Contact).action == Action::None);
	c = o.Update(1, 1, Kind::None);
	KT1_CHECK(c.action == Action::Clear && c.slot == 0);
	KT1_CHECK_EQ(o.Holes(), 1);
	KT1_CHECK(o.Update(1, 1, Kind::Full).action == Action::Write); // back in sight: its own slot again
	KT1_CHECK_EQ(o.Holes(), 0);
}

KT1_CASE("ICOLEX-ORDER-REMOVE", "a removed unit leaves a hole; its id reused by a newer unit is appended") {
	IconOrder o = Made({{1, 1, Kind::Full}, {2, 2, Kind::Full}});
	auto c = o.Remove(1);
	KT1_CHECK(c.action == Action::Clear && c.slot == 0);
	KT1_CHECK_EQ(o.SlotOf(1), IconOrder::NONE);
	KT1_CHECK(o.Remove(1).action == Action::None);
	c = o.Update(1, 5, Kind::Full);
	KT1_CHECK(c.action == Action::Append && c.slot == 2);
	o.Rebuild(std::vector<Entry>{{2, 2, Kind::Full}, {1, 5, Kind::Full}});
	KT1_CHECK_EQ(o.Count(), 2);
	KT1_CHECK_EQ(o.Holes(), 0);
	KT1_CHECK_EQ(o.SlotOf(1), 1);
}

KT1_CASE("ICOLEX-ORDER-RUNS", "contact runs: stretches of neighbouring contact slots, updated after changes") {
	IconOrder o = Made({{1, 1, Kind::Contact}, {2, 2, Kind::Contact}, {3, 3, Kind::Full}, {4, 4, Kind::Contact}});
	auto r = o.ContactRuns();
	KT1_CHECK_EQ(r.size(), size_t(2));
	KT1_CHECK(r[0].first == 0 && r[0].count == 2);
	KT1_CHECK(r[1].first == 3 && r[1].count == 1);
	o.Update(3, 3, Kind::Contact);
	r = o.ContactRuns();
	KT1_CHECK_EQ(r.size(), size_t(1));
	KT1_CHECK(r[0].first == 0 && r[0].count == 4);
}

KT1_CASE("ICOLEX-ORDER-CAP", "past the slot cap the oldest units keep their icons; a unit listed twice is drawn once") {
	IconOrder o;
	o.Init(100, 2);
	const bool fits = o.Rebuild(std::vector<Entry>{{5, 9, Kind::Full}, {6, 1, Kind::Full}, {7, 4, Kind::Full}, {6, 1, Kind::Full}});
	KT1_CHECK(!fits);
	KT1_CHECK_EQ(o.Count(), 2);
	KT1_CHECK_EQ(o.IdAt(0), 6);
	KT1_CHECK_EQ(o.IdAt(1), 7);
	KT1_CHECK_EQ(o.SlotOf(5), IconOrder::NONE);
	KT1_CHECK(o.Update(8, 10, Kind::Full).action == Action::Rebuild); // full: no slot to append into
}

KT1_CASE("ICOLEX-ORDER-NOALLOC", "steady state: rebuild, updates, removes and runs allocate nothing after Init") {
	IconOrder o;
	o.Init(4096, 4096);
	std::vector<Entry> units;
	for (int i = 0; i < 2000; ++i)
		units.push_back({i, static_cast<uint32_t>(i + 1), (i % 7 == 0) ? Kind::Contact : Kind::Full});
	o.Rebuild(units);
	o.ContactRuns();
	const long long before = KT1::Test::AllocationCount();
	for (int pass = 0; pass < 3; ++pass) {
		o.Rebuild(units);
		for (int i = 0; i < 2000; i += 3)
			o.Update(i, static_cast<uint32_t>(i + 1), Kind::Contact);
		for (int i = 1; i < 2000; i += 50)
			o.Remove(i);
		o.Update(3000 + pass, static_cast<uint32_t>(5000 + pass), Kind::Full);
		o.ContactRuns();
	}
	KT1_CHECK_EQ(KT1::Test::AllocationCount(), before);
}
