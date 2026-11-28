/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Core/IconLexicon tests: HitFlicker, the red window of a damaged unit's icon (LUA-FREE-WORKPACKAGES D01; GAME-DESIGN 1).
// The window is draw time in seconds, so the same hit gives the same red at any drawn frame rate and any sim rate.

#include "../KT1Test.h"
#include "Game/KT1/Core/IconLexicon/HitFlicker.h"

using namespace KT1::Core::IconLexicon;

namespace {
	constexpr float WINDOW = 0.15f; // DESIGN (owner rule: about 0.15 s)
}

KT1_CASE("ICOLEX-FLICK-WINDOW", "a hit paints red once, then restores once the window is over") {
	HitFlicker f;
	f.Init(100, WINDOW);
	f.Hit(7, 10.0);
	auto w = f.Tick(10.0);
	KT1_CHECK(w.size() == 1 && w[0].id == 7 && w[0].red);
	KT1_CHECK(f.Tick(10.1).empty());                 // still red: nothing to send
	w = f.Tick(10.2);
	KT1_CHECK(w.size() == 1 && w[0].id == 7 && !w[0].red);
	KT1_CHECK_EQ(f.Active(), 0);
	KT1_CHECK(f.Tick(10.3).empty());
}

KT1_CASE("ICOLEX-FLICK-RESTART", "a second hit inside the window restarts it") {
	HitFlicker f;
	f.Init(100, WINDOW);
	f.Hit(3, 0.0);
	f.Tick(0.0);
	f.Hit(3, 0.1);
	KT1_CHECK(f.Tick(0.2).empty());                  // 0.1 s after the second hit
	KT1_CHECK_EQ(f.Tick(0.3).size(), size_t(1));
}

KT1_CASE("ICOLEX-FLICK-REBUILT", "a rebuild or rewrite puts the own colour back: red is applied again inside the window") {
	HitFlicker f;
	f.Init(100, WINDOW);
	f.Hit(1, 0.0);
	f.Hit(2, 0.0);
	f.Tick(0.0);
	f.Rebuilt();
	KT1_CHECK_EQ(f.Tick(0.05).size(), size_t(2));
	f.Rewritten(2);
	auto w = f.Tick(0.06);
	KT1_CHECK(w.size() == 1 && w[0].id == 2 && w[0].red);
}

KT1_CASE("ICOLEX-FLICK-SLOWFRAME", "a frame slower than the window: an entry seen first after it ends sends nothing") {
	HitFlicker f;
	f.Init(100, WINDOW);
	f.Hit(4, 0.0);
	KT1_CHECK(f.Tick(0.25).empty());
	KT1_CHECK_EQ(f.Active(), 0);
}

KT1_CASE("ICOLEX-FLICK-FORGET", "a dead unit sends no write and leaves no entry; ids out of range are ignored") {
	HitFlicker f;
	f.Init(10, WINDOW);
	f.Hit(5, 0.0);
	f.Hit(6, 0.0);
	f.Forget(5);
	f.Hit(-1, 0.0);
	f.Hit(10, 0.0);
	auto w = f.Tick(0.0);
	KT1_CHECK(w.size() == 1 && w[0].id == 6);
}

KT1_CASE("ICOLEX-FLICK-NOALLOC", "steady state: 2,000 hits a second allocate nothing after Init") {
	HitFlicker f;
	f.Init(4096, WINDOW);
	const long long before = KT1::Test::AllocationCount();
	double t = 0;
	for (int frame = 0; frame < 120; ++frame, t += 1.0 / 60) {
		for (int i = 0; i < 34; ++i)
			f.Hit((frame * 34 + i) % 4096, t);
		f.Tick(t);
	}
	KT1_CHECK_EQ(KT1::Test::AllocationCount(), before);
}
