/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 core: the sound cues on their way from the sim to the audio side, as typed display events (LUA-FREE H01, FRAME §4.6;
// they replace the messages "snd", "snd_loop" and "snd_alert" of snd_events). No engine include.
// The sim sends them with DisplayEvents::Send; the audio side registers with DisplaySink::Listen<T>
// (Draw/Common/DisplayListen.h) and decides what the viewer hears.

#ifndef KT1_CORE_CUE_EVENTS_H
#define KT1_CORE_CUE_EVENTS_H

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string_view>

#include "Game/KT1/Core/Common/DisplayTypes.h"
#include "Game/KT1/Core/SimHost/DisplayQueue.h"

namespace KT1::Core::Cues {

constexpr size_t CUE_TEXT = 32;        // DESIGN: longest cue, variant or alert kind name, with its end
constexpr size_t SEVERITY_TEXT = 16;   // DESIGN: longest alert severity

// a name into an event's field; a longer one is cut
template<size_t N> inline void SetName(char (&field)[N], std::string_view name) {
	const size_t n = name.size() < N - 1 ? name.size() : N - 1;
	if (n > 0) std::memcpy(field, name.data(), n);
	field[n] = 0;
}

// one cue at a unit or a point
struct CueEvent {
	static constexpr int32_t TYPE = Core::DisplayRange::FRAME + 0;
	int32_t unit = 0;                  // 0: a cue at a point
	int32_t def = 0;                   // the unit's def; 0: none
	int32_t team = 0;
	int32_t weapon = 0;                // the weapon def of a fire or impact cue; 0: none
	float pos[3] = {0.0f, 0.0f, 0.0f};
	char cue[CUE_TEXT] = {0};
	char variant[CUE_TEXT] = {0};
};

// a simulation alert for one ally team (UI-GAP-001, UI-GAP-012); not counted against the cue limit
struct CueAlertEvent {
	static constexpr int32_t TYPE = Core::DisplayRange::FRAME + 1;
	int32_t allyTeam = 0;
	float x = 0.0f, z = 0.0f;          // 0, 0: no position
	char kind[CUE_TEXT] = {0};
	char severity[SEVERITY_TEXT] = {0};   // empty: the kind's own
};

// a builder's construction loop starts or stops; not counted against the cue limit, so a stop is never lost
struct CueLoopEvent {
	static constexpr int32_t TYPE = Core::DisplayRange::FRAME + 2;
	int32_t unit = 0;
	uint8_t on = 0;
};

static_assert(sizeof(CueEvent) <= Core::SimHost::DISPLAY_TEXT);
static_assert(sizeof(CueAlertEvent) <= Core::SimHost::DISPLAY_TEXT);
static_assert(sizeof(CueLoopEvent) <= Core::SimHost::DISPLAY_TEXT);

} // namespace KT1::Core::Cues

#endif
