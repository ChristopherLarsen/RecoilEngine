/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

/* SupCom (RND-005 v2 R13): the fade fraction of the per-decal cut-off. A Lua decal with a cut-off (user data 0 y) starts fading at
 * fade x its cut-off and is not drawn at the cut-off (GroundDecalsVertProg.glsl, uniform cutFade). Set by Spring.KT1.Decals.SetFade. Draw only. */
#ifndef SUPCOM_DECAL_CUT_H
#define SUPCOM_DECAL_CUT_H

namespace SupComDecalCut {
	inline constexpr float DEFAULT_FADE = 0.25f;   // PARITY (RND-005 v2 R13)
	inline float fade = DEFAULT_FADE;
	// KT1 zoom gate (owner 2026-11-27: decals are not drawn beyond the closest zoom levels): while true the ground decal pass is skipped;
	// the decals stay (new ones are still made and aged). Set by Spring.KT1.Decals.SetHidden from unsynced Lua. Draw only.
	inline bool hidden = false;
}

#endif
