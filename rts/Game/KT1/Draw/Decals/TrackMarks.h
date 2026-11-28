/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 FxDecals: footprints, hover scuffs, downwash rings and the engine's tread switch (replaces
// LuaRules/Gadgets/gfx_tracks.lua, and the drawn-side half of gamedata/track_classes.lua and loco_gait.lua). Looks only.
// A unit's class comes from its def facts (Core::Fx::UnitDefFx); foot positions from Snapshot<Core::Fx::TrackSample>.
// Footfall dust is asked of the unit effects (Draw/UnitFx/UnitFxApi.h Foot) for every contact returned. Engine-free.

#ifndef KT1_TRACK_MARKS_H
#define KT1_TRACK_MARKS_H

#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "Game/KT1/Core/Common/Column.h"
#include "Game/KT1/Core/Common/DenseIndex.h"
#include "Game/KT1/Core/FxEvents/FxSamples.h"
#include "Game/KT1/Draw/Decals/DecalRequest.h"

namespace KT1::FxDecals {

struct GaitRow {
	int legs = 4;                 // PLACEHOLDER
	bool thirds = false;          // PLACEHOLDER: three legs a third of a cycle apart; else alternate
	float print = 1.0f;           // PLACEHOLDER: print width factor
	int print_every = 1;          // PLACEHOLDER: below High a print for every n-th contact
	template<class S> void Describe(S& s) {
		s.Field("legs", legs);
		s.Field("thirds", thirds);
		s.Field("print", print);
		s.Field("print_every", print_every);
	}
};

struct SizeRow {
	float long_side = 0.0f;       // PLACEHOLDER: units whose longest side is at most this; 0: any longer
	float life_factor = 1.0f;     // PLACEHOLDER
	template<class S> void Describe(S& s) {
		s.Field("long_side", long_side);
		s.Field("life_factor", life_factor);
	}
};

struct FactionTex {
	std::string tread = "kt_tread_%s";    // DESIGN: %s: the faction's track code
	std::string foot = "kt_foot_%s";      // DESIGN
	std::string tyre = "kt_tyre";         // DESIGN
	std::string scuff = "kt_hover_scuff"; // DESIGN
	std::string downwash = "kt_downwash"; // DESIGN
	std::string code_human = "hum";       // DESIGN: the faction codes the %s takes
	std::string code_arachnid = "arach";  // DESIGN
	std::string code_grey = "grey";       // DESIGN
	std::string code_hadean = "had";      // DESIGN
	template<class S> void Describe(S& s) {
		s.Field("tread", tread);
		s.Field("foot", foot);
		s.Field("tyre", tyre);
		s.Field("scuff", scuff);
		s.Field("downwash", downwash);
		s.Field("code_human", code_human);
		s.Field("code_arachnid", code_arachnid);
		s.Field("code_grey", code_grey);
		s.Field("code_hadean", code_hadean);
	}
};

struct BiomeTint {
	std::string biome;            // PLACEHOLDER
	float rgb[3] = {0.23f, 0.23f, 0.14f};   // PLACEHOLDER
	template<class S> void Describe(S& s) {
		s.Field("biome", biome);
		s.Field("rgb", rgb);
	}
};

struct TrackBudgetConfig {
	float low_cap = 4000.0f;          // PLACEHOLDER: alive marks at scale 1 (below it, in proportion)
	float low_per_step = 25.0f;       // PLACEHOLDER: new marks per step at scale 1
	float low_nearest = 300.0f;       // PLACEHOLDER: engine tracks: the units nearest the view centre, at scale 1
	float low_print_life = 0.75f;     // PLACEHOLDER
	float high_cap_add = 6000.0f;     // PLACEHOLDER: more at scale 2
	float high_per_step_add = 55.0f;  // PLACEHOLDER
	float high_print_life_add = 0.25f;    // PLACEHOLDER
	float all_scale = 2.0f;           // PLACEHOLDER: from this scale: every unit's tracks, downwash, no thinning
	template<class S> void Describe(S& s) {
		s.Field("low_cap", low_cap);
		s.Field("low_per_step", low_per_step);
		s.Field("low_nearest", low_nearest);
		s.Field("low_print_life", low_print_life);
		s.Field("high_cap_add", high_cap_add);
		s.Field("high_per_step_add", high_per_step_add);
		s.Field("high_print_life_add", high_print_life_add);
		s.Field("all_scale", all_scale);
	}
};

struct TrackConfig {                  // game/data/kt1/decals.json
	float life_prints = 12.0f;        // DESIGN: owner 2026-10-27 (D3), seconds
	float life_scuff = 3.0f;          // DESIGN: owner 2026-10-27 (D3)
	float life_downwash = 1.5f;       // PLACEHOLDER
	float foot_width = 0.35f;         // PLACEHOLDER: print width over unit width, x the gait's factor
	float foot_min = 4.0f;            // PLACEHOLDER: elmos
	float foot_aspect = 1.4f;         // PLACEHOLDER: length over width
	float foot_side = 0.3f;           // PLACEHOLDER: without foot positions: this share of the width to each side
	float foot_rows = 0.6f;           // PLACEHOLDER: and rows over this share of the length
	float stride_min = 6.0f;          // DESIGN: ANM-001 art tuning, elmos
	float contact_phase = 0.25f;      // PLACEHOLDER: a leg touches down at this phase
	float contact_height = 4.0f;      // PLACEHOLDER: a unit this near the ground leaves prints
	float walk_every_s = 0.1f;        // DESIGN: each walker is followed this often, staggered by id
	float scuff_size = 1.3f;          // PLACEHOLDER: times the longest side
	float scuff_every = 1.0f;         // PLACEHOLDER: body lengths between scuffs
	float ring_size = 2.2f;           // PLACEHOLDER: downwash ring times the longest side
	float max_height = 60.0f;         // PLACEHOLDER: rotors higher than this leave none
	float min_height = 3.0f;          // PLACEHOLDER
	float ring_grow = 0.5f;           // PLACEHOLDER: the ring grows by this share at max_height
	float ring_fade = 0.6f;           // PLACEHOLDER: and fades by this share
	float downwash_every_s = 0.5f;    // PLACEHOLDER
	float print_alpha = 0.85f;        // PLACEHOLDER
	float scuff_alpha = 0.7f;         // PLACEHOLDER
	float wash_alpha = 0.6f;          // PLACEHOLDER
	float tint_alpha = 0.5f;          // PLACEHOLDER
	float view_margin = 400.0f;       // PLACEHOLDER: elmos round a unit that must be in view
	float state_every_s = 1.0f;       // DESIGN: the option, the tread switch and the fade distance are looked at this often
	float icon_size_factor = 1.75f;   // DESIGN: the engine's screen icon size factor for our smallest icon
	float fade_from = 0.7f;           // DESIGN: marks start to fade at this share of the far distance
	float icon_dist_factor = 27.386f; // DESIGN: icon distance setting to elmos (the root of 750)
	std::string setting = "KardashevTrackScale";   // DESIGN
	std::string mod_option = "track_scale";        // DESIGN
	std::vector<GaitRow> gaits = {{2, false, 1.00f, 1}, {3, true, 1.00f, 1}, {4, false, 1.00f, 1}, {6, false, 0.85f, 2},
		{8, false, 0.75f, 2}, {10, false, 0.65f, 3}, {12, false, 0.60f, 3}};   // PLACEHOLDER: gamedata/loco_gait.lua
	std::vector<SizeRow> sizes = {{10.0f, 0.8f}, {22.0f, 1.0f}, {44.0f, 1.3f}, {0.0f, 1.8f}};   // PLACEHOLDER: s, m, l, x
	std::vector<BiomeTint> tints = {{"meadow", {0.23f, 0.23f, 0.14f}}, {"tundra", {0.37f, 0.50f, 0.62f}},
		{"dunes", {0.54f, 0.38f, 0.25f}}, {"badlands", {0.29f, 0.24f, 0.20f}}, {"ember", {0.08f, 0.055f, 0.11f}}};   // PLACEHOLDER
	FactionTex textures;
	TrackBudgetConfig budget;
	template<class S> void Describe(S& s) {
		s.Field("life_prints", life_prints);
		s.Field("life_scuff", life_scuff);
		s.Field("life_downwash", life_downwash);
		s.Field("foot_width", foot_width);
		s.Field("foot_min", foot_min);
		s.Field("foot_aspect", foot_aspect);
		s.Field("foot_side", foot_side);
		s.Field("foot_rows", foot_rows);
		s.Field("stride_min", stride_min);
		s.Field("contact_phase", contact_phase);
		s.Field("contact_height", contact_height);
		s.Field("walk_every_s", walk_every_s);
		s.Field("scuff_size", scuff_size);
		s.Field("scuff_every", scuff_every);
		s.Field("ring_size", ring_size);
		s.Field("max_height", max_height);
		s.Field("min_height", min_height);
		s.Field("ring_grow", ring_grow);
		s.Field("ring_fade", ring_fade);
		s.Field("downwash_every_s", downwash_every_s);
		s.Field("print_alpha", print_alpha);
		s.Field("scuff_alpha", scuff_alpha);
		s.Field("wash_alpha", wash_alpha);
		s.Field("tint_alpha", tint_alpha);
		s.Field("view_margin", view_margin);
		s.Field("state_every_s", state_every_s);
		s.Field("icon_size_factor", icon_size_factor);
		s.Field("fade_from", fade_from);
		s.Field("icon_dist_factor", icon_dist_factor);
		s.Field("setting", setting);
		s.Field("mod_option", mod_option);
		s.List("gaits", gaits, [](auto& o, GaitRow& r) { r.Describe(o); });
		s.List("sizes", sizes, [](auto& o, SizeRow& r) { r.Describe(o); });
		s.List("tints", tints, [](auto& o, BiomeTint& t) { t.Describe(o); });
		s.Object("textures", [&](auto& o) { textures.Describe(o); });
		s.Object("budget", [&](auto& o) { budget.Describe(o); });
	}
};

struct TrackBudget {
	int32_t cap = 0, perStep = 0, nearest = 0;   // nearest -1: every unit
	float printLife = 0.0f;
	bool downwash = false, thin = true;
};

enum class TrackKind : uint8_t { None, Treads, Prints, Scuff, Downwash };

struct LiveMark { uint32_t handle = 0; float expires = 0.0f; };

struct TrackStats {
	uint32_t created = 0, dropped = 0, capped = 0, prints = 0, scuffs = 0, washes = 0, tracksOn = 0, frames = 0;
	float costMs = 0.0f;
};

struct TrackState {
	Core::DenseIndex units;                          // units that leave something, by unit id
	Core::Column<uint8_t> kind;                      // TrackKind
	Core::Column<int32_t> defId;
	Core::Column<float> x, z, phase, travelled, next;
	Core::Column<uint8_t> engineOn;                  // treads: the engine's tracks are switched on
	Core::Column<uint32_t> steps[Core::Fx::TRACK_FEET];   // contacts of each leg
	std::vector<LiveMark> prints, shortMarks;  // alive, in creation order
	std::vector<int32_t> switchOn, switchOff;  // the last tread switch's changes
	KT1::FxDraw::Random random;
	TrackBudget budget;
	float scale = 1.0f, nextState = 0.0f;
	int32_t step = -1, made = 0;
	TrackStats stats;
};

struct WalkerIn {                              // a unit of the state, as it is now
	int32_t unit = -1;
	float x = 0.0f, y = 0.0f, z = 0.0f, frontX = 0.0f, frontZ = 1.0f, groundY = 0.0f;
	bool showable = false;                     // seen, near the view, no icon, not carried
	bool takesTracks = true;                   // the ground type
	const Core::Fx::UnitDefFx* def = nullptr;
	const Core::Fx::TrackSample* feet = nullptr;   // nullptr: none published
};
struct Contact { int32_t unit = -1; float x = 0.0f, y = 0.0f, z = 0.0f; uint8_t size = 0; int32_t legs = 0, step = 0; };

TrackBudget BudgetOf(const TrackConfig& c, float scale);
TrackKind KindOf(const Core::Fx::UnitDefFx& def);
void InitTracks(TrackState& s, const TrackConfig& c, int32_t maxUnits);
void AddUnit(TrackState& s, const TrackConfig& c, int32_t unit, int32_t defId, const Core::Fx::UnitDefFx& def, float x, float z);
void RemoveUnit(TrackState& s, int32_t unit);
// one step: walkers (their contacts are appended to `contacts`), hovers and rotors
void StepTracks(TrackState& s, const TrackConfig& c, std::span<const WalkerIn> units, int32_t step, float now,
	std::vector<Contact>& contacts, DecalOutput& out);
// treaders: which get the engine's tracks (not icons; below the all scale the nearest the view centre); `candidates`
// are the units not drawn as icons with their squared distance to the centre
struct TreadCandidate { int32_t unit = -1; float d2 = 0.0f; };
void SwitchTreads(TrackState& s, std::span<TreadCandidate> candidates, bool decalsOn);
void ExpireTracks(TrackState& s, float now, DecalOutput& out);
void ForgetMarks(TrackState& s);               // the engine's decals were switched: the handles are gone

} // namespace KT1::FxDecals

#endif
