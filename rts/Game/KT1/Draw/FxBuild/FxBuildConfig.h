/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 FxBuild: the system's config (game/data/kt1/fxbuild.json). Engine-free.

#ifndef KT1_FX_BUILD_CONFIG_H
#define KT1_FX_BUILD_CONFIG_H

#include "Game/KT1/Core/FxEvents/FxCriteria.h"
#include "Game/KT1/Draw/FxBuild/BuildBeams.h"
#include "Game/KT1/Draw/FxBuild/CriteriaFx.h"
#include "Game/KT1/Draw/FxBuild/FogPuff.h"
#include "Game/KT1/Draw/FxBuild/ReclaimStream.h"
#include "Game/KT1/Draw/FxBuild/SiteGhost.h"

namespace KT1::FxBuild {

struct FxBuildConfig {
	BeamsConfig beams;
	CriteriaConfig criteria;
	Core::Fx::CriteriaTimingConfig timing;    // the same values as fxevents.json "criteria"
	ShakeConfig shake;
	ReclaimConfig reclaim;
	FogPuffConfig fog_puff;
	SiteGhostConfig site_ghost;
	template<class S> void Describe(S& s) {
		s.Object("beams", [&](auto& o) { beams.Describe(o); });
		s.Object("criteria", [&](auto& o) { criteria.Describe(o); });
		s.Object("timing", [&](auto& o) { timing.Describe(o); });
		s.Object("shake", [&](auto& o) { shake.Describe(o); });
		s.Object("reclaim", [&](auto& o) { reclaim.Describe(o); });
		s.Object("fog_puff", [&](auto& o) { fog_puff.Describe(o); });
		s.Object("site_ghost", [&](auto& o) { site_ghost.Describe(o); });
	}
};

} // namespace KT1::FxBuild

#endif
