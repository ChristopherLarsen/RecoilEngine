/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 FxFields: shields, drawn as one instanced sphere mesh (replaces LuaUI/fx/shield_fx.lua's shield rules and
// LuaRules/Gadgets/fx_shields.lua). Fed by the events ShieldState and ShieldHit (Core/FxEvents/FxEvent.h). Engine-free.

#ifndef KT1_FIELD_SHIELDS_H
#define KT1_FIELD_SHIELDS_H

#include <cstdint>
#include <string>
#include <vector>

#include "Game/KT1/Core/Common/Column.h"
#include "Game/KT1/Core/Common/DenseIndex.h"
#include "Game/KT1/Core/FxEvents/FxEvent.h"
#include "Game/KT1/Draw/FxCommon/FxShapes.h"

namespace KT1::FxFields {

using Core::Fx::Vec3;
using KT1::FxDraw::Rgb;

constexpr int32_t SHIELD_HITS = 4;       // DESIGN: ripples per shield at once (the instance carries them)

// one instance, 36 floats (shader locations 1 .. 9)
struct ShieldInstance {
	float posRad[4];             // centre (dome: base), radius
	float fill[4];               // rgb, base alpha
	float rim[4];                // rgb, look (0 Human, 1 Arachnid, 2 Grey, 3 Hadean)
	float state[4];              // assemble 0 .. 1, down progress (-1 none), flicker, dome (0 sphere; 1 + cut share)
	float state2[4];             // down kind (1 collapse, 2 fade), up, health, seed
	float hit[SHIELD_HITS][4];   // direction from the centre, age 0 .. 1 (-1 none)
};
static_assert(sizeof(ShieldInstance) == 144);

struct ShieldStyle {
	int look = 0;                // DESIGN
	float color[3] = {0.498f, 0.827f, 1.0f};   // DESIGN
	float alpha = 0.30f;         // DESIGN
	template<class S> void Describe(S& s) {
		s.Field("look", look);
		s.Field("color", color);
		s.Field("alpha", alpha);
	}
};

struct ShieldConfig {            // game/data/kt1/fxfields.json "shields"
	int max = 1024;              // DESIGN: shields drawn
	int rings = 16;              // DESIGN: the mesh
	int sectors = 32;            // DESIGN
	float assemble_s = 0.9f;     // DESIGN
	float ripple_life_s = 0.6f;  // DESIGN: the combat values' look.shield_impact_life wins (ANM-004 v2 R5)
	float idle_opacity = 0.1f;   // DESIGN: ANM-004 v2 SF-ANM-4
	float collapse_life_s = 0.7f;      // DESIGN
	float power_down_life_s = 0.45f;   // DESIGN
	float flicker_from = 0.5f;   // DESIGN
	float flicker_max = 0.85f;   // DESIGN
	float warn_from = 0.45f;     // DESIGN
	float warn_to = 0.1f;        // DESIGN
	float collapse_below = 0.05f;      // DESIGN
	float collapse_hit_window_s = 0.3f;   // DESIGN
	float team_tint = 0.45f;     // DESIGN
	float warn[3] = {1.0f, 0.36f, 0.30f};   // DESIGN
	float hemi_min_up = 0.05f;   // DESIGN: a dome's ripple at least this far up
	float personal_radius = 1.15f;     // DESIGN: of the unit's radius
	float transport_radius = 1.1f;     // DESIGN
	// DESIGN (the style sheets, section 8; Human's colour REQ HUMAN-STYLE s.8), by Core::Fx::Faction
	ShieldStyle human = {0, {0.498f, 0.827f, 1.0f}, 0.30f};
	ShieldStyle arachnid = {1, {0.957f, 0.945f, 0.918f}, 0.22f};
	ShieldStyle grey = {2, {0.784f, 0.722f, 1.0f}, 0.24f};
	ShieldStyle hadean = {3, {0.878f, 0.255f, 0.165f}, 0.20f};
	template<class S> void Describe(S& s) {
		s.Field("max", max);
		s.Field("rings", rings);
		s.Field("sectors", sectors);
		s.Field("assemble_s", assemble_s);
		s.Field("ripple_life_s", ripple_life_s);
		s.Field("idle_opacity", idle_opacity);
		s.Field("collapse_life_s", collapse_life_s);
		s.Field("power_down_life_s", power_down_life_s);
		s.Field("flicker_from", flicker_from);
		s.Field("flicker_max", flicker_max);
		s.Field("warn_from", warn_from);
		s.Field("warn_to", warn_to);
		s.Field("collapse_below", collapse_below);
		s.Field("collapse_hit_window_s", collapse_hit_window_s);
		s.Field("team_tint", team_tint);
		s.Field("warn", warn);
		s.Field("hemi_min_up", hemi_min_up);
		s.Field("personal_radius", personal_radius);
		s.Field("transport_radius", transport_radius);
		s.Object("human", [&](auto& o) { human.Describe(o); });
		s.Object("arachnid", [&](auto& o) { arachnid.Describe(o); });
		s.Object("grey", [&](auto& o) { grey.Describe(o); });
		s.Object("hadean", [&](auto& o) { hadean.Describe(o); });
	}
};

enum class ShieldDown : uint8_t { None, Collapse, Power };
enum class ShieldChange : uint8_t { None, Assemble, Collapse, PowerDown };

struct ShieldHitMark { Vec3 dir; float time = 0.0f; uint8_t used = 0; };

struct Shield {
	bool up = false;
	float fraction = 1.0f;
	Core::Fx::ShieldKind kind = Core::Fx::ShieldKind::Bubble;
	Core::Fx::Faction faction = Core::Fx::Faction::Human;
	float radius = 0.0f, height = 0.0f;
	float upSince = -100.0f, downSince = -100.0f, lastHit = -100.0f, seed = 0.0f;
	ShieldDown down = ShieldDown::None;
	ShieldHitMark hits[SHIELD_HITS];
};

struct ShieldStats { uint32_t drawn = 0, draws = 0, ripples = 0, skipped = 0, collapses = 0, fades = 0, breaker = 0; };

struct ShieldState {
	Core::DenseIndex units;                    // by unit id
	Core::Column<Shield> shields;
	std::vector<uint8_t> ripples;        // by weapon def id: the ripples its look asks (a shield breaker); 1: normal
	KT1::FxDraw::InstanceList<ShieldInstance> instances;
	bool hidden = false;                 // test hook: draw nothing
	ShieldStats stats;
};

struct ShieldPlace {                     // where a shield's unit is this frame
	Vec3 pos, mid;
	float unitRadius = 0.0f;
	Rgb teamColour;
};

void InitShields(ShieldState& s, const ShieldConfig& c, int32_t maxUnits);
// ShieldState event; up -1 removes the shield. Returns what changed
ShieldChange OnShieldState(ShieldState& s, const ShieldConfig& c, const Core::Fx::ShieldState& e, float now);
// a hit ripple at the point (hemi: a dome, from its sphere's centre); returns the slot, -1: none
int32_t OnShieldHit(ShieldState& s, const ShieldConfig& c, int32_t unit, const Vec3& point, const Vec3& centre, float now);
const ShieldStyle& StyleOf(const ShieldConfig& c, Core::Fx::Faction faction);
float ShieldOpacity(const ShieldConfig& c, float fraction);
float ShieldFlicker(const ShieldConfig& c, float fraction);
Rgb ShieldTint(const ShieldConfig& c, Core::Fx::Faction faction, float fraction);
Rgb ShieldRim(const ShieldConfig& c, const Rgb& fill, const Rgb& team);
float RippleAge(const ShieldConfig& c, const Shield& shield, int32_t slot, float now);   // -1: none
float Assemble(const ShieldConfig& c, const Shield& shield, float now);
float Down(const ShieldConfig& c, const Shield& shield, float now);                      // -1: none
bool Animating(const ShieldConfig& c, const Shield& shield, float now);
bool ShieldVisible(const ShieldConfig& c, const Shield& shield, float now, bool detail);
float DrawRadius(const ShieldConfig& c, const Shield& shield, float unitRadius);
float DomeCode(const Shield& shield, float radius);
ShieldInstance PackShield(const ShieldConfig& c, const Shield& shield, const ShieldPlace& place, float now, bool detail);

} // namespace KT1::FxFields

#endif
