/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 draw: stubs of IconSet.h and IconsBridge.h (LUA-FREE H01). UNITDRAW steps D01 and D02 write the bodies.

#include "IconSet.h"
#include "IconsBridge.h"
#include "Game/KT1/Core/IconLexicon/IconHud.h"

namespace KT1::Icons {

const Core::IconLexicon::IconState& Set() {
	static const Core::IconLexicon::IconState none;
	return none;
}

const Core::IconLexicon::IconConfig& Config() {
	static const Core::IconLexicon::IconConfig none;
	return none;
}

const Core::IconLexicon::IconClass* ClassOf(int) { return nullptr; }
std::span<const Core::IconLexicon::IconClass> Classes() { return {}; }

const Core::IconLexicon::HudOptions& Options() {
	static const Core::IconLexicon::HudOptions none;
	return none;
}

void SetOptions(const Core::IconLexicon::HudOptions&) {}
float SwitchDistance() { return -1; }
bool PastSwitch() { return false; }
std::span<const Core::IconLexicon::IconRect> Rects() { return {}; }
int HitAt(int, int) { return -1; }
int Hovered() { return -1; }
void SetPaused(bool) {}
bool Paused() { return false; }
bool FlickerTest(int) { return false; }

} // namespace KT1::Icons

namespace KT1 {

void PushIconsBridge(lua_State*) {}

} // namespace KT1
