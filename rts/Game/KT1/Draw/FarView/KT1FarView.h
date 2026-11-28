/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 FarView: far above the map, the world pass draws a low-resolution textured map mesh instead of the full scene.
// Included by rts/Rendering/WorldDrawer.cpp ("// KT1 hook: FarView") and rts/Rendering/Env/Particles/ProjectileDrawer.cpp ("// KT1 hook: FarView fx"). See KT1FarView.cpp.

#ifndef KT1_FAR_VIEW_H
#define KT1_FAR_VIEW_H

namespace KT1 {
namespace FarView {
	// One line in CWorldDrawer::Draw: true when the far view drew this frame's opaque and alpha world passes
	// (the caller then skips DrawOpaqueObjects and DrawAlphaObjects). False when the rule is off or the camera is below the switch.
	bool Draw();
	// the game camera's zoom distance this drawn frame, elmos (ViewSettings); below 0: unset
	void SetZoom(float zoom);
	// Effects in the far view (owner 2026-11-26, D-Z3 withdrawn: shots and explosions follow the original at L4+).
	// True when the last drawn frame was far and this effect lies past its view-depth cutoff (WVFX-ZOOM-001 v7 R3, R4):
	// `weapon` is a weapon projectile (its own cutoff), else a CEG particle or a ground flash. Called by
	// rts/Rendering/Env/Particles/ProjectileDrawer.cpp ("// KT1 hook: FarView fx" lines). Always false in the full view.
	bool FxCulled(float viewDepth, bool weapon);
	// the last drawn frame was the far view (Spring.KT1.FarView.IsFar; the icon set forces icons on then)
	bool IsFar();
}
}

#endif
