/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 audio: the unsynced half of the sound cues (LUA-FREE D10): hears the sim's cue events, holds back the ones the
// sound manager would drop at this camera height, and hands the rest to its LuaUI globals (SoundCue, SoundBuildLoop,
// SoundAlert). The manager still decides what the viewer hears.

#ifndef KT1_AUDIO_CUE_AUDIO_H
#define KT1_AUDIO_CUE_AUDIO_H

#include <functional>
#include <string>
#include <string_view>

namespace KT1::CueAudio {

void Install();                                          // game setup: registers the listeners, forgets the gate
// elmos above the ground from which the manager drops every item of `cue`; 0 or less: never held back
void SetZoomCut(std::string_view cue, float height);
// every cue held back since the last call, with its number
void TakeKept(const std::function<void(const std::string&, unsigned)>& fn);

} // namespace KT1::CueAudio

#endif
