/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 EFFECTS draw: the viewer's switches, budget and sight. Stub (LUA-FREE H01): step D03 writes the bodies.

#include "Game/KT1/Draw/FxCommon/FxView.h"

#include "Game/KT1/Draw/Common/FrameFacts.h"

namespace KT1::FxDraw {

void FxView::Init(const FxViewConfig&) {}
void FxView::Frame(const FrameFacts&) {}
SwitchState FxView::State(Switch) const { return {}; }
bool FxView::Admit(Core::Fx::Priority) { return false; }
float FxView::Scale() const { return 0.0f; }
bool FxView::Sees(Core::Fx::SeenMask) const { return false; }
bool FxView::SeesAlly(int32_t) const { return false; }
bool FxView::FullView() const { return false; }
int32_t FxView::LocalTeam() const { return 0; }
int32_t FxView::LocalAllyTeam() const { return 0; }
void FxView::Reset() {}

FxView& View() {
	static FxView view;
	return view;
}

} // namespace KT1::FxDraw
