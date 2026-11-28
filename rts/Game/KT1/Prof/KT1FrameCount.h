/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 FrameCount: dev-only frame counters, off by default (NEW-ENGINE-PLAN r6 Phase 0 item 5; game-build docs/engine/FRAMECOUNT.md).
// Unsynced, never touches sim state. Stock files reach it through KT1Registry.h ("// KT1 hook: FrameCount").
//
// Switches (springsettings, so a broker manifest sets them in "config"):
//   KT1FrameCount=1             count (0, the default: nothing is counted and nothing is printed)
//   KT1FrameCountFrames=a-b     count from game frame a to game frame b, then print (empty: from the first frame to the end)
// Lua (any handle): Spring.KT1.FrameCount.Start([tag]) prints any open window and opens a new one now, tagged (default "lua"; the
// config window is "cfg"); .Stop() prints the open window now; .IsOn(). Start does nothing while KT1FrameCount=0.
// Output: exactly one "[FRAMECOUNT] ..." infolog line per window (at b, at Stop, or at game end if neither came).

#ifndef KT1_FRAMECOUNT_H
#define KT1_FRAMECOUNT_H

struct lua_State;

namespace KT1 {
namespace FrameCount {
	void SimBegin();               // Game.cpp SimFrame, after gs->frameNum advanced
	void SimEnd();                 // Game.cpp SimFrame, after lastSimFrameTime
	void DrawFrame();              // Game.cpp Draw, first line: one drawn frame (also in a headless engine: one main-loop pass)
	void Finish(const char* why);  // Game.cpp ~CGame: prints an open window
	void PushLua(lua_State* L);    // KT1Registry PushLua: Spring.KT1.FrameCount
}
}

#endif
