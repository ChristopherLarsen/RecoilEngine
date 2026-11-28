/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Ribbons: its config.
// Engine-free. Values: game-build game/data/kt1/ (full labels there). The members hold the defaults, each
// with its source label.

#ifndef KT1_RIBBONS_CONFIG_H
#define KT1_RIBBONS_CONFIG_H

#include <string>
#include <vector>

#include "Game/KT1/Data/Schema.h"

namespace KT1 {

struct RibbonsConfig {
	struct Cap { float scale = 1; int ribbons = 200, points = 12; };
	// PLACEHOLDER: ribbons alive and points per ribbon, by effect scale (caps and the Cap defaults)
	std::vector<Cap> caps = {{0.5f, 60, 6}, {1, 200, 12}, {2, 600, 20}};
	float nearD = 1500, midD = 3500, farD = 6000;   // PLACEHOLDER (each of the three): elmos: beyond mid half the points, beyond far no ribbon
	float shortLife = 0.5f;      // DESIGN: seconds, the longest ribbon at the "short" setting
	float guardLife = 0.5f;      // DESIGN: life share while the frame rate guard limits
	int extrasMax = 40;          // PLACEHOLDER: shots in view above which the extras are dropped
	int lifeMax = 120;           // PLACEHOLDER: shots in view above which lives shorten
	float lifeK = 0.5f;          // PLACEHOLDER: by this share
	int defaultPriority = 9;     // DESIGN: priority of a weapon that names none
	int poolMax = 8192;          // DESIGN: segments alive per core pool
	int extraPoolMax = 2048;     // DESIGN: per dash or thread pool
	int headMax = 2048;          // DESIGN: heads per pool and drawn frame
	float settingsEveryS = 1;    // DESIGN: seconds between settings reads
	std::string texHot = "bitmaps/sp_fx/sp_ribbon_hot.tga";   // DESIGN
	std::string texSmoke = "bitmaps/sp_fx/sp_ribbon_smoke.tga";   // DESIGN

	void Describe(Data::Schema& s) {
		s.List("caps", caps, [](Data::Schema& c, Cap& cap) {
			c.Field("scale", cap.scale);
			c.Field("ribbons", cap.ribbons, Data::AtLeast(0));
			c.Field("points", cap.points, Data::AtLeast(1));
		});
		s.Field("near", nearD);
		s.Field("mid", midD);
		s.Field("far", farD);
		s.Field("short_life", shortLife);
		s.Field("guard_life", guardLife);
		s.Object("heavy", [this](Data::Schema& h) {
			h.Field("extras", extrasMax);
			h.Field("life", lifeMax);
			h.Field("lifeK", lifeK);
		});
		s.Field("default_priority", defaultPriority);
		s.Field("pool_max", poolMax, Data::AtLeast(1));
		s.Field("extra_pool_max", extraPoolMax, Data::AtLeast(1));
		s.Field("head_max", headMax, Data::AtLeast(1));
		s.Field("settings_every_s", settingsEveryS, Data::AtLeast(0));
		s.Object("default_tex", [this](Data::Schema& t) {
			t.Field("hot", texHot);
			t.Field("smoke", texSmoke);
		});
	}
};

} // namespace KT1

#endif
