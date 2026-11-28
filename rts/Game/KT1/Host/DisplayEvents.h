/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Host: the only way a synced system causes a display effect (SIM-DRAW-PARALLEL 13.4, 13.8). No engine include.
//
// Sim side: Emit during the step (a system in check mode emits nothing: Check::MayWrite); the events are published,
// stamped with the step, at its end (Phases, FrameEnd).
// Draw side: the draw host calls Replay once per drawn frame, in its Update phase; the events of every step published
// since the last call come in step order, each step's in emit order. Nothing in SimFrame calls Replay, and nothing in
// the sim reads the queue back. The sink that plays sounds and effects is draw-side code (Host/DisplaySink.cpp, F06);
// without it the published events are dropped, oldest first, beyond Core/SimHost's cap.

#ifndef KT1_HOST_DISPLAY_EVENTS_H
#define KT1_HOST_DISPLAY_EVENTS_H

#include "Game/KT1/Core/SimHost/DisplayQueue.h"

class float3;

namespace KT1 {

class DisplayEvents {
public:
	static bool Emit(const Core::SimHost::DisplayEvent& e);   // false: skipped (check mode)

	static void Sound(int soundId, const float3& pos, float volume);
	static void Effect(int generatorId, const float3& pos, const float3& dir, float damage, float radius, int ownerId);
	static void Log(const char* text);
	static void DebugLine(const float3& from, const float3& to, float width, float seconds);
	static void Ping(const float3& pos, const char* label, int playerId);
	// a family's typed event (Core/Common/DisplayTypes.h)
	template<class T> static bool Send(const T& payload) {
		Core::SimHost::DisplayEvent e;
		e.SetPayload(payload);
		return Emit(e);
	}

	static void Publish(int step);   // the step's end (Phases)
	// Draw side only: drains every published step through the sink, then forgets them.
	static void Replay(void (*sink)(const Core::SimHost::DisplayEvent&));
	static void Reset();             // game shutdown
};

} // namespace KT1

#endif
