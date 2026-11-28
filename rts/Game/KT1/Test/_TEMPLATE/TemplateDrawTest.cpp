/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// Core/_TEMPLATE/TemplateDrawCore.h, engine-free, with the two-step Host/Snapshot it reads. Copy to
// Test/<System>/<System>Test.cpp.

#include "../KT1Test.h"

#include "Game/KT1/Core/_TEMPLATE/TemplateDrawCore.h"
#include "Game/KT1/Host/Snapshot.h"

using namespace KT1::Core::Template;

KT1_CASE("TEMPLATE-DRAW-BLEND", "a dot moves from the previous step to the current one by the step fraction") {
	KT1::Snapshot<Dot> s;
	s.Init(8);
	s.Begin(1); s.Put(2, 1, {0, 0, 0, 0}); s.Commit();
	s.Begin(2); s.Put(2, 1, {8, 4, 2, 6}); s.Commit();
	const Dot half = Blend(s.Previous(2), *s.Current(2), 0.5f);
	KT1_CHECK_EQ(half.x, 4.0f);
	KT1_CHECK_EQ(half.charge, 3.0f);
	KT1_CHECK_EQ(Blend(s.Previous(2), *s.Current(2), 1.5f).x, 8.0f);   // never past the current step
}

KT1_CASE("TEMPLATE-DRAW-REUSE", "a new unit with a dead unit's id never blends from the dead unit's place") {
	KT1::Snapshot<Dot> s;
	s.Init(8);
	s.Begin(1); s.Put(2, 1, {100, 0, 0, 0}); s.Commit();
	s.Begin(2); s.Put(2, 2, {8, 0, 0, 0}); s.Commit();
	KT1_CHECK(s.Previous(2) == nullptr);
	KT1_CHECK_EQ(Blend(s.Previous(2), *s.Current(2), 0.0f).x, 8.0f);
}

KT1_CASE("TEMPLATE-DRAW-ALPHA", "opacity follows the charge and stays within 0 and full") {
	KT1_CHECK_EQ(static_cast<int>(Alpha(0, 10, 200)), 0);
	KT1_CHECK_EQ(static_cast<int>(Alpha(5, 10, 200)), 100);
	KT1_CHECK_EQ(static_cast<int>(Alpha(20, 10, 200)), 200);
	KT1_CHECK_EQ(static_cast<int>(Alpha(1, 0, 200)), 200);
}
