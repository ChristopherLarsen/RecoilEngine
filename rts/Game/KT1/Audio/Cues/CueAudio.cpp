/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 audio: CueAudio.h. The same three LuaUI calls snd_events made, with the same arguments.

#include "CueAudio.h"

#include <algorithm>
#include <map>

#include "Game/Camera.h"
#include "Game/KT1/Core/Cues/CueEvents.h"
#include "Game/KT1/Core/Cues/CueState.h"
#include "Game/KT1/Draw/Common/DisplayListen.h"
#include "Lua/LuaUI.h"
#include "Map/Ground.h"
#include "lib/lua/include/LuaInclude.h"

namespace KT1::CueAudio {
namespace {

namespace Cue = Core::Cues;

struct Gate {
	float cut = 0.0f;
	unsigned kept = 0;
};

std::map<std::string, Gate, std::less<>> gates;

float CameraHeight() {
	const float3 pos = CCamera::GetActive()->GetPos();
	return pos.y - CGround::GetHeightReal(pos.x, pos.z, false);
}

bool Held(std::string_view cue) {
	const auto it = gates.find(cue);
	if (it == gates.end() || !Cue::ZoomHeld(CameraHeight(), it->second.cut)) return false;
	++it->second.kept;
	return true;
}

void Play(const Cue::CueEvent& e) {
	if (luaUI == nullptr || Held(e.cue)) return;
	luaUI->KT1CallGlobal("SoundCue", [&e](lua_State* L) {
		lua_pushstring(L, e.cue);
		lua_pushinteger(L, e.unit);
		lua_pushinteger(L, e.def);
		lua_pushinteger(L, e.team);
		for (const float p : e.pos) lua_pushnumber(L, p);
		lua_pushstring(L, e.variant);
		lua_pushinteger(L, e.weapon);
		return 9;   // DESIGN: SoundCue's arguments
	});
}

void Loop(const Cue::CueLoopEvent& e) {
	if (luaUI == nullptr) return;
	luaUI->KT1CallGlobal("SoundBuildLoop", [&e](lua_State* L) {
		lua_pushinteger(L, e.unit);
		lua_pushboolean(L, e.on != 0);
		return 2;   // DESIGN: SoundBuildLoop's arguments
	});
}

void Alert(const Cue::CueAlertEvent& e) {
	if (luaUI == nullptr) return;
	luaUI->KT1CallGlobal("SoundAlert", [&e](lua_State* L) {
		lua_pushstring(L, e.kind);
		lua_pushstring(L, e.severity);
		lua_pushnumber(L, e.x);
		lua_pushnumber(L, e.z);
		lua_pushinteger(L, e.allyTeam);
		return 5;   // DESIGN: SoundAlert's arguments
	});
}

} // namespace

void Install() {
	gates.clear();
	DisplaySink::Listen<Cue::CueEvent>([](const Cue::CueEvent& e) { Play(e); });
	DisplaySink::Listen<Cue::CueLoopEvent>([](const Cue::CueLoopEvent& e) { Loop(e); });
	DisplaySink::Listen<Cue::CueAlertEvent>([](const Cue::CueAlertEvent& e) { Alert(e); });
}

void SetZoomCut(std::string_view cue, float height) {
	if (!cue.empty()) gates[std::string(cue)].cut = std::max(0.0f, height);
}

void TakeKept(const std::function<void(const std::string&, unsigned)>& fn) {
	for (auto& [cue, gate] : gates) {
		if (gate.kept > 0) fn(cue, gate.kept);
		gate.kept = 0;
	}
}

} // namespace KT1::CueAudio
