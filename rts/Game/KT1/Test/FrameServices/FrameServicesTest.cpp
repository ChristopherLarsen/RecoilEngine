/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 frame services (LUA-FREE V01): the change counters, the ray latch and the shared selection.

#include <vector>

#include "Game/KT1/Core/DrawHost/SelectionTracker.h"
#include "Game/KT1/Core/ViewSettings/FrameChanges.h"
#include "Game/KT1/Core/ViewSettings/FrameLatch.h"
#include "Game/KT1/Test/KT1Test.h"

using namespace KT1::Core::View;

KT1_CASE("FRAME-NAMES", "the five parts by name; any other name is none") {
	KT1_CHECK(ParseFrameWhat("selection") == FrameWhat::Selection);
	KT1_CHECK(ParseFrameWhat("hover") == FrameWhat::Hover);
	KT1_CHECK(ParseFrameWhat("camera") == FrameWhat::Camera);
	KT1_CHECK(ParseFrameWhat("zoom") == FrameWhat::Zoom);
	KT1_CHECK(ParseFrameWhat("mouse") == FrameWhat::Mouse);
	KT1_CHECK(ParseFrameWhat("Selection") == FrameWhat::Count);
	KT1_CHECK(ParseFrameWhat("") == FrameWhat::Count);
}

KT1_CASE("FRAME-CHANGES", "the first note counts, then only a differing one; parts are apart; reset forgets") {
	FrameChanges c;
	KT1_CHECK_EQ(c.Count(FrameWhat::Hover), 0u);
	c.Note(FrameWhat::Hover, false);
	KT1_CHECK_EQ(c.Count(FrameWhat::Hover), 1u);
	c.Note(FrameWhat::Hover, false);
	KT1_CHECK_EQ(c.Count(FrameWhat::Hover), 1u);
	c.Note(FrameWhat::Hover, true);
	KT1_CHECK_EQ(c.Count(FrameWhat::Hover), 2u);
	KT1_CHECK_EQ(c.Count(FrameWhat::Camera), 0u);
	c.Note(FrameWhat::Count, true);                    // no such part
	KT1_CHECK_EQ(c.Count(FrameWhat::Count), 0u);
	c.Reset();
	KT1_CHECK_EQ(c.Count(FrameWhat::Hover), 0u);
}

KT1_CASE("FRAME-LATCH", "one ray per key: again when the frame, step, pixel, view, camera or minimap moves") {
	FrameLatch<RayKey> latch;
	RayKey k;
	KT1_CHECK(latch.Moved(k));
	KT1_CHECK(!latch.Moved(k));
	k.drawFrame = 1;
	KT1_CHECK(latch.Moved(k));
	k.step = 1;
	KT1_CHECK(latch.Moved(k));
	k.x = 1;
	KT1_CHECK(latch.Moved(k));
	k.y = 1;
	KT1_CHECK(latch.Moved(k));
	k.view[2] = 1920;
	KT1_CHECK(latch.Moved(k));
	k.cam[6] = 45.0f;
	KT1_CHECK(latch.Moved(k));
	k.miniMap[2] = 200;
	KT1_CHECK(latch.Moved(k));
	KT1_CHECK(!latch.Moved(k));
	latch.Reset();
	KT1_CHECK(latch.Moved(k));
}

KT1_CASE("FRAME-SELECTION", "the selection is ascending ids; the counter moves only when the set differs") {
	KT1::SelectionTracker s;
	s.Init(256);
	KT1_CHECK(!s.Update(std::vector<int>{}));
	KT1_CHECK(s.Update(std::vector<int>{9, 3, 200}));
	KT1_CHECK_EQ(s.Ids().size(), size_t(3));
	KT1_CHECK_EQ(s.Ids()[0], 3);
	KT1_CHECK_EQ(s.Ids()[2], 200);
	KT1_CHECK(!s.Update(std::vector<int>{200, 9, 3}));
	KT1_CHECK_EQ(s.Changes(), 1u);
	KT1_CHECK(s.Update(std::vector<int>{3}));
	KT1_CHECK(s.Contains(3));
	KT1_CHECK(!s.Contains(9));
}
