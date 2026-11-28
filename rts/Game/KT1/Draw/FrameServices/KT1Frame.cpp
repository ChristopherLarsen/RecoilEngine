/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 draw system: Frame (version 1). One copy of what the widgets that still run poll for themselves: the selection,
// what is under the pointer, the camera, the zoom level and the mouse (LUA-FREE V01, FRAME §4.1). It draws nothing and
// runs nothing on its own: a reader's call brings its part up to date (FrameRefresh.h).
// Replaces:   the engine calls of the every-frame widgets (LuaUI/lib/frame_facts.lua picks the bridge when on)
// Switch:     mod option kt1_frame, else kt1_fam_frame; default off
// Config:     none
// Doc:        game-build docs/engine/rules/frame.md

#include "FrameBridge.h"
#include "FrameRefresh.h"

#include "Game/KT1/KT1Register.h"

namespace KT1 {
namespace FrameServices {
namespace {

class FrameRule : public Rule {
public:
	FrameRule() : Rule({"Frame", 1, false, "(widget polling: selection, pointer, camera)", false}) { Reset(); }
	void PushLua(lua_State* L) override { PushFrameBridge(L); }
};

} // namespace
} // namespace FrameServices

// ORDER: the first draw system; later ones may read its state
using FrameRuleType = FrameServices::FrameRule;
KT1_REGISTER_SYSTEM(Frame, 6000, FrameRuleType)

} // namespace KT1
