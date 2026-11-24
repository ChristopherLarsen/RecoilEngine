/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 FarView: far above the map, the world pass draws a low-resolution textured map mesh instead of the full scene.
// Included only by rts/Rendering/WorldDrawer.cpp (its "// KT1 hook: FarView" line). See KT1FarView.cpp.

#ifndef KT1_FAR_VIEW_H
#define KT1_FAR_VIEW_H

namespace KT1 {
namespace FarView {
	// One line in CWorldDrawer::Draw: true when the far view drew this frame's opaque and alpha world passes
	// (the caller then skips DrawOpaqueObjects and DrawAlphaObjects). False when the rule is off or the camera is below the switch.
	bool Draw();
}
}

#endif
