/* This file is part of the Recoil engine fork of Kardashev Type One (GPL v2 or later). */
// Shared zoom level L0..L7 (KardashevGameBuild docs/engine/ZOOM-TIER-PLAN.md 2b). PRESENTATION ONLY: unsynced code reads
// it; synced code must never read it (PRF-002 R3/R6). Updated once per drawn frame at the start of CGame::Draw.
#pragma once

namespace SupComZoomLevel {
	enum Pass : unsigned {
		Units       = 1u << 0,   // unit models, opaque and alpha
		Features    = 1u << 1,   // feature models, opaque and alpha
		Projectiles = 1u << 2,   // projectile models, sprites and ground flashes
		Particles   = 1u << 3,   // nano particles
		Decals      = 1u << 4,   // ground decals (scars, tracks, build plates)
		Foliage     = 1u << 5,   // grass
		Sky         = 1u << 6,
	};
	constexpr int MAX_LEVEL = 7;
	constexpr int SKIP_FROM = 4;   // icon_zoom: L4 and out

	void Init();                   // reads springsetting SupComZoomSkip (mask of Pass; default 0 = nothing skipped)
	void Update();                 // from the active camera; fires SupComZoomLevelChanged on a change
	int   Get();
	float Dist();
	bool  Skip(Pass p);            // p is in the skip mask and the level is SKIP_FROM or more
}
