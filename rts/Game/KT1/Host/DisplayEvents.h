/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Host: the only way a synced system causes a display effect (SIM-DRAW-PARALLEL 13.4, 13.8).
//
// Emit during the step; the events are published at FrameEnd and replayed, in order, once the step's work is done.
// While the game is fast-forwarding, sounds and pings are dropped. Nothing may read the queue back.
// The queue has one consumer, Replay, called once per sim step with or without a draw side, so it never grows.

#ifndef KT1_HOST_DISPLAY_EVENTS_H
#define KT1_HOST_DISPLAY_EVENTS_H

#include "Game/KT1/Core/SimHost/DisplayQueue.h"

class float3;

namespace KT1 {

class DisplayEvents {
public:
	static void Emit(const Core::SimHost::DisplayEvent& e);

	static void Sound(int soundId, const float3& pos, float volume);
	static void Effect(int generatorId, const float3& pos, const float3& dir, float damage, float radius, int ownerId);
	static void Log(const char* text);
	static void DebugLine(const float3& from, const float3& to, float width, float seconds);
	static void Ping(const float3& pos, const char* label, int playerId);

	static void Publish();                  // the step's end (Phases)
	static void Replay(bool skipping);      // KT1 hook: SimHost (CGame::SimFrame, after the step)
	static void Reset();                    // game shutdown
};

} // namespace KT1

#endif
