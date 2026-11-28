/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1: Spring.KT1.Cues for the Lua that still runs, synced and unsynced (LUA-FREE H01, FRAME §4.5). The raising calls
// replace GG.SoundEvent, GG.SoundEventAt and GG.SoundAlert; the fact calls replace what snd_events read from other
// gadgets' messages and tables. The table is a rule's: Spring.KT1.<rule name>, so the rule whose PushLua calls this is
// registered as "Cues". The audio side hands what it plays to the sound manager widget through its LuaUI globals
// (SoundCue, SoundBuildLoop, SoundAlert), as snd_events did: no bridge call for that.
//
// Synced (a call from an unsynced handle does nothing and returns false):
//   Raise(unitID, cue [, variant])                      -> bool                 (GG.SoundEvent)
//   RaiseAt(cue, x, y, z [, team [, variant]])          -> bool                 (GG.SoundEventAt)
//   Alert(kind, severity, x, z, allyTeam)               -> nothing; x, z may be nil   (GG.SoundAlert)
//   Loop(unitID, on)                                    -> nothing: a builder's loop (message "snd_loop")
//   ShieldHit(unitID, x, y, z)                          -> nothing              (message "shield_hit")
//   ShieldState(unitID, up, reason, height)             -> nothing              (message "shield_state")
//   CaptureProgress(targetID, captorID, progress)       -> nothing              (GG.Eco.CaptureProgress)
//   CaptureDone(oldUnitID, newUnitID)                   -> nothing              (GG.Eco.events "captured")
//   Counts()                                            -> sent, cut
// Unsynced:
//   SetZoomCut(cue, height | nil)                       -> nothing: the camera height above ground from which the manager
//                                                          drops every item of the cue (LuaUI/lib/snd_gate.lua)
//   Kept()                                              -> { cue = number held back since the last call }
//                                                          (replaces SoundCueGate's arguments)

#ifndef KT1_AUDIO_CUE_BRIDGE_H
#define KT1_AUDIO_CUE_BRIDGE_H

struct lua_State;

namespace KT1 {

void PushCuesBridge(lua_State* L);   // fills the table on top of the stack

} // namespace KT1

#endif
