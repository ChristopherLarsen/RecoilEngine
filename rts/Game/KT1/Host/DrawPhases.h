/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 host: the draw phases. A draw system is added once and is then called in the phases it names, in (order, name)
// order, each call under the timer "KT1::<Name>", before the Lua call-in of the same phase.
//
// Phases and where they run (DrawPhase, Core/DrawHost/PhaseList.h):
//   Update, Genesis, ScreenEffects, Screen   a native event client before every Lua handle (no stock file touched)
//   PreDraw                                  a second event client, last in Update: after every Lua Update, before the
//                                            frame's camera->Update and world update
//   WorldPreUnit, World, WorldAlpha          hook lines in WorldDrawer ("// KT1 hook: DrawPhases")
//   MiniMap                                  one hook line in MiniMap, with Lua's DrawInMiniMap matrices
//   FrameEnd                                 one hook line in GlobalRendering::SwapBuffers, once per drawn frame, after
//                                            everything drawn (also with the interface hidden)
// With no system added (and no sim display events in the engine) nothing is registered and each hook is one test of a
// flag: the frame is the stock frame.
//
// Update also plays the sim's published display events (Host/DisplaySink.h), before any system's Update.
//
// A system gets a DrawContext: the DrawView and the FrameFacts, both const, and nothing else of the game. It leaves the
// GL state as it found it (GL::SubState). In the screen phases the matrices are Lua's (view pixels, y up); in MiniMap
// they are Lua's too (0..miniMapW, 0..miniMapH).
//
// Switching: Active() is asked at each PreDraw. A system that becomes active again gets Resumed() first and must draw a
// correct frame at once (the capture proof switches systems off and on inside one run).

#ifndef KT1_HOST_DRAWPHASES_H
#define KT1_HOST_DRAWPHASES_H

#include "Game/KT1/Core/DrawHost/PhaseList.h"
#include "Game/KT1/Draw/Common/FrameFacts.h"
#include "Game/KT1/Host/DrawView.h"

namespace KT1 {

struct DrawContext {
	DrawPhase phase;
	const DrawView& view;
	const FrameFacts& facts;
	int miniMapW = 0;   // MiniMap phase only
	int miniMapH = 0;
};

class DrawSystem {
public:
	virtual ~DrawSystem() = default;
	virtual const char* Name() const = 0;          // the timer "KT1::<Name>"
	virtual unsigned Phases() const = 0;           // PhaseBit(...) | ...
	virtual int Order() const { return 0; }        // lower draws first
	virtual bool Active() const { return true; }   // a registry rule returns Enabled()
	virtual void Resumed() {}                      // active again after frames without it: forget what may be old
	virtual void Draw(const DrawContext& ctx) = 0;
	virtual void Kill() {}                         // the game ends: release GL objects
};

class DrawPhases {
public:
	static void Init();                            // KT1 hook: DrawPhases (CWorldDrawer::InitPost)
	// a switched-on system adds itself once per game; the host does not own it
	static void Add(DrawSystem* system);

	static void Run(DrawPhase phase);              // KT1 hook: DrawPhases (WorldDrawer, MiniMap)
	static void WorldAlpha(bool aboveWater);       // KT1 hook: DrawPhases (WorldDrawer; runs once, in the above-water pass)
	static void FrameEnd();                        // KT1 hook: DrawPhases (GlobalRendering::SwapBuffers)
	static void Kill();                            // KT1 hook: DrawPhases (CWorldDrawer::Kill)

	// the facts for script and tests: those of the last DrawGenesis; with no active system, filled now for the frame
	static const FrameFacts& Facts();
	static void SetPointer(int x, int y);
	static void ClearPointer();
};

} // namespace KT1

#endif
