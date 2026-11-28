/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// Core/_TEMPLATE/TemplateRuleCore.h, engine-free. Copy to Test/<System>/<System>Test.cpp: the rule is proved here, at 30
// and at 10 steps per second, before any engine build.

#include "../KT1Test.h"

#include "Game/KT1/Core/Common/Rate.h"
#include "Game/KT1/Core/_TEMPLATE/TemplateRuleCore.h"
#include "Game/KT1/Data/Config.h"
#include "Game/KT1/Rules/_TEMPLATE/TemplateRuleConfig.h"

using namespace KT1::Core::Template;
using KT1::Core::Rate;
using KT1::Core::SimHost::IdRange;
using KT1::Test::AllocationCount;

namespace {
	constexpr int UNITS = 16;

	void Run(RuleState& s, const RuleParams& p, int steps) {
		for (int i = 0; i < steps; ++i) s.Visit(s.TakeDue(), IdRange::All(), p);
	}

	float ChargeOf(RuleState& s, int id) { return s.charge[s.units.index.SlotOf(id)]; }
}

KT1_RATE_CASE("TEMPLATE-RULE-PERIOD", "a unit is visited once per period in seconds, at any step rate") {
	const RuleParams p{Rate::Every(0.5), 1, 100};
	RuleState s;
	s.Init(UNITS, p);
	s.Add(7, p);
	Run(s, p, Rate::Steps(2.0));
	KT1_CHECK_EQ(ChargeOf(s, 7), 4.0f);
}

KT1_CASE("TEMPLATE-RULE-CAP", "the charge stops at the cap") {
	const RuleParams p{1, 4, 10};
	RuleState s;
	s.Init(UNITS, p);
	s.Add(3, p);
	Run(s, p, 5);
	KT1_CHECK_EQ(ChargeOf(s, 3), 10.0f);
}

KT1_CASE("TEMPLATE-RULE-REUSE", "a new unit with a dead unit's id starts blank, on its own period") {
	const RuleParams p{4, 1, 10};
	RuleState s;
	s.Init(UNITS, p);
	s.Add(5, p);
	Run(s, p, 5);
	const auto first = s.serial[s.units.index.SlotOf(5)];
	s.Remove(5);
	s.Add(5, p);
	KT1_CHECK_EQ(ChargeOf(s, 5), 0.0f);
	KT1_CHECK(s.serial[s.units.index.SlotOf(5)] != first);
	Run(s, p, 3);
	KT1_CHECK_EQ(ChargeOf(s, 5), 0.0f);   // the dead unit's visit is gone
	Run(s, p, 1);
	KT1_CHECK_EQ(ChargeOf(s, 5), 1.0f);
}

KT1_CASE("TEMPLATE-RULE-NOALLOC", "no allocation once the wheel has turned") {
	const RuleParams p{4, 1, 10};
	RuleState s;
	s.Init(UNITS, p);
	for (int id = 0; id < UNITS; ++id) s.Add(id, p);
	Run(s, p, 20);
	const auto before = AllocationCount();
	Run(s, p, 40);
	KT1_CHECK_EQ(AllocationCount(), before);
}

KT1_CASE("TEMPLATE-RULE-CONFIG", "the file's keys fill the config; a key it leaves out keeps its default") {
	KT1::TemplateRuleConfig c;
	KT1_CHECK(KT1::Data::ParseInto(R"({"period_s": 2, "gain": 3})", KT1::Data::Strictness::Dev, c).accepted);
	KT1_CHECK_EQ(c.periodS, 2.0f);
	KT1_CHECK_EQ(c.gain, 3.0f);
	KT1_CHECK_EQ(c.cap, 10.0f);
}
