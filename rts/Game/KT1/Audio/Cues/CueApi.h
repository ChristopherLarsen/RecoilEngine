/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1: raising sound cues, for every native system and (through CueBridge.h) for the Lua that still runs
// (LUA-FREE H01, FRAME §4.5; replaces GG.SoundEvent, GG.SoundEventAt and GG.SoundAlert of snd_events). Bodies are FRAME
// step D10's; until then nothing is raised.
// Called from the sim: a cue becomes a typed display event (Core/Cues/CueEvents.h), so a system in check mode raises
// nothing. A cue name is any event name of the sound data.
// Lint: the plan puts this header under Audio/, so a file in Rules/ includes it with
// "KT1 lint: allow sim-draw-include cue API" on the line; the bodies read units, so they are not written in Audio/.

#ifndef KT1_AUDIO_CUE_API_H
#define KT1_AUDIO_CUE_API_H

#include <string_view>

class float3;

namespace KT1::Cues {

// at the unit's position, for those who may see it. False: no live unit, or over the cue limit (counted).
bool Raise(std::string_view cue, int unitId, std::string_view variant = {});
// at a point; team: whose cue it is, 0 as Lua's nil
bool RaiseAt(std::string_view cue, const float3& pos, int team = 0, std::string_view variant = {});
// an alert for one ally team, of the kind's own severity and without a position
void Alert(std::string_view kind, int allyTeam);
void AlertAt(std::string_view kind, std::string_view severity, float x, float z, int allyTeam);

// facts other systems own, which the cue rules turn into cues
void ShieldHit(int unitId, const float3& pos);
void ShieldState(int unitId, int up, std::string_view reason, float height);   // up: 1 up, 0 down, -1 gone
void CaptureProgress(int targetId, int captorId, float progress);
void CaptureDone(int oldUnitId, int newUnitId);                                // plays capture_complete at the new unit

} // namespace KT1::Cues

#endif
