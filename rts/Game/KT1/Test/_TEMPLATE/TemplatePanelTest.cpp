/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// Core/_TEMPLATE/TemplatePanelCore.h, engine-free. Copy to Test/<System>/<System>Test.cpp.

#include "../KT1Test.h"

#include "Game/KT1/Core/_TEMPLATE/TemplatePanelCore.h"

#include <initializer_list>

using namespace KT1::Core::Template;
using KT1::Test::AllocationCount;

namespace {
	void Fill(PanelModel& m, uint32_t changes, std::initializer_list<int32_t> defs) {
		m.DefIds().assign(defs);
		m.Rebuild(changes);
	}
}

KT1_CASE("TEMPLATE-PANEL-ROWS", "one row per unit def, ascending, with its count") {
	PanelModel m;
	m.Init(16, 8);
	Fill(m, 1, {9, 2, 9, 9, 2});
	KT1_CHECK((m.Rows() == std::vector<SelectionRow>{{2, 2}, {9, 3}}));
	KT1_CHECK_EQ(m.LeftOut(), 0);
}

KT1_CASE("TEMPLATE-PANEL-LIMIT", "defs past the row limit are left out and counted") {
	PanelModel m;
	m.Init(16, 1);
	Fill(m, 1, {4, 7, 4, 8});
	KT1_CHECK((m.Rows() == std::vector<SelectionRow>{{4, 2}}));
	KT1_CHECK_EQ(m.LeftOut(), 2);
}

KT1_CASE("TEMPLATE-PANEL-STALE", "rebuilt only when the selection changes, or after Forget") {
	PanelModel m;
	m.Init(16, 8);
	KT1_CHECK(m.Stale(0));
	Fill(m, 0, {1});
	KT1_CHECK(!m.Stale(0));
	KT1_CHECK(m.Stale(1));
	m.Forget();
	KT1_CHECK(m.Stale(0));
}

KT1_CASE("TEMPLATE-PANEL-NOALLOC", "no allocation after Init") {
	PanelModel m;
	m.Init(16, 8);
	Fill(m, 1, {3, 1, 3});
	m.DefIds().clear();
	const auto before = AllocationCount();
	for (uint32_t change = 2; change < 20; ++change) {
		m.DefIds().clear();
		m.DefIds().push_back(static_cast<int32_t>(change));
		m.Rebuild(change);
	}
	KT1_CHECK_EQ(AllocationCount(), before);
}
