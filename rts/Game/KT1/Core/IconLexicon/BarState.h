/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 core: the in-world status bars and marks: config, instance layout, state and rules (LUA-FREE H01, UNITDRAW §4.6;
// replaces the widget hud_bars, UI-GAP-002 v4). No engine include: it lives under Core/IconLexicon so the bar geometry
// is tested engine-free (UNITDRAW §7); the adapter header is Draw/UnitBars/BarInstance.h. Config data/kt1/unitbars.json.
// Coordinates are view px, y up, snapped to whole pixels as the widget did. Check column "instances".

#ifndef KT1_CORE_UNIT_BAR_STATE_H
#define KT1_CORE_UNIT_BAR_STATE_H

#include <cstdint>
#include <string>
#include <vector>

#include "HudOptions.h"

namespace KT1::Core::UnitBars {

struct BarConfig {
	int max_bars = 300;                       // PLACEHOLDER (R5 thinning cap): hovered and selected units win
	float ref_distance = 600.0f;              // DESIGN: unit-to-camera distance of the base size
	float min_factor = 0.7f;                  // DESIGN: size limits as a factor of the base
	float max_factor = 1.25f;                 // DESIGN
	float hide_zoom = 1584.0f;                // REQ CAM-003 v4 R2 bar_hide_zoom (198 map units): no bars from this camera zoom on
	std::string hide_sweep = "bars";          // REQ RND-006 v2 R14: the level-of-detail sweep that scales it
	float healthy_from = 0.6f;                // REQ UI-GAP-002 v4 R7
	float damaged_from = 0.3f;                // REQ UI-GAP-002 v4 R7
	float width = 44.0f;                      // DESIGN: health bar at 100 % UI
	float height[2] = {5.0f, 3.0f};           // DESIGN: health bar height and its minimum
	float thin_height[2] = {3.0f, 2.0f};      // DESIGN: shield, fuel and progress bar height and its minimum
	float gap = 2.0f;                         // DESIGN: px between stacked bars
	float border = 1.0f;                      // DESIGN: the dark frame around a bar
	float critical_mark[2] = {4.0f, 2.0f};    // REQ UI-GAP-002 v4 R7: the marker's left and right, x size left of the bar
	float vet_offset = 4.0f;                  // DESIGN: first veterancy mark right of the bar, x size
	float vet_pitch = 5.0f;                   // DESIGN
	float vet_width = 3.0f;                   // DESIGN
	float lift = 6.0f;                        // DESIGN: elmos above the def's height
	float default_height = 20.0f;             // DESIGN: elmos, for a def without a height
	float text_size = 12.0f;                  // DESIGN: the self-destruct countdown
	float text_offset = 4.0f;                 // DESIGN: above the bar
	float text_min_alpha = 0.6f;              // DESIGN
	float background_alpha = 0.7f;            // DESIGN
	float fill_alpha = 0.95f;                 // DESIGN
	float healthy[3] = {0.3f, 0.85f, 0.35f};  // PLACEHOLDER (art pass); a chosen palette replaces these three
	float damaged[3] = {0.95f, 0.75f, 0.2f};  // PLACEHOLDER
	float critical[3] = {0.95f, 0.25f, 0.2f}; // PLACEHOLDER
	float shield[3] = {0.8f, 0.8f, 0.85f};    // PLACEHOLDER
	float shield_down_alpha = 0.6f;           // DESIGN (2026-10-30): opacity factor of a down shield's bar
	float shield_track_alpha = 0.2f;          // DESIGN (2026-10-30): the empty track of a down shield
	float fuel[3] = {0.9f, 0.6f, 0.25f};      // PLACEHOLDER
	float fuel_low[3] = {0.95f, 0.35f, 0.2f}; // PLACEHOLDER
	float fuel_low_fraction = 0.19f;          // DESIGN (2026-11-10): where a def sets none (spc_fuel_low_fraction)
	float progress[3] = {0.92f, 0.86f, 0.7f}; // PLACEHOLDER
	float veterancy[3] = {0.95f, 0.85f, 0.3f};    // PLACEHOLDER
	float countdown[3] = {1.0f, 0.35f, 0.25f};    // PLACEHOLDER
	template<class S> void Describe(S& s) {
		s.Field("max_bars", max_bars);
		s.Field("ref_distance", ref_distance);
		s.Field("min_factor", min_factor);
		s.Field("max_factor", max_factor);
		s.Field("hide_zoom", hide_zoom);
		s.Field("hide_sweep", hide_sweep);
		s.Field("healthy_from", healthy_from);
		s.Field("damaged_from", damaged_from);
		s.Field("width", width);
		s.Field("height", height);
		s.Field("thin_height", thin_height);
		s.Field("gap", gap);
		s.Field("border", border);
		s.Field("critical_mark", critical_mark);
		s.Field("vet_offset", vet_offset);
		s.Field("vet_pitch", vet_pitch);
		s.Field("vet_width", vet_width);
		s.Field("lift", lift);
		s.Field("default_height", default_height);
		s.Field("text_size", text_size);
		s.Field("text_offset", text_offset);
		s.Field("text_min_alpha", text_min_alpha);
		s.Field("background_alpha", background_alpha);
		s.Field("fill_alpha", fill_alpha);
		s.Field("healthy", healthy);
		s.Field("damaged", damaged);
		s.Field("critical", critical);
		s.Field("shield", shield);
		s.Field("shield_down_alpha", shield_down_alpha);
		s.Field("shield_track_alpha", shield_track_alpha);
		s.Field("fuel", fuel);
		s.Field("fuel_low", fuel_low);
		s.Field("fuel_low_fraction", fuel_low_fraction);
		s.Field("progress", progress);
		s.Field("veterancy", veterancy);
		s.Field("countdown", countdown);
	}
};

// One solid rectangle as the GPU reads it: two vec4 at locations 1..2, 32 bytes. Drawn in array order with straight
// alpha blending, so the order is the widget's.
struct BarInstance {
	float rect[4] = {0.0f, 0.0f, 0.0f, 0.0f};     // location 1: x0, y0, x1, y1, view px
	float colour[4] = {0.0f, 0.0f, 0.0f, 0.0f};   // location 2: rgba
};

static_assert(sizeof(BarInstance) == 32, "the bar shader reads two vec4");

// a countdown's text, centred at x, drawn after the rectangles of its unit
struct BarText {
	int32_t unit = -1;
	int32_t seconds = 0;
	float x = 0.0f, y = 0.0f, size = 0.0f;
	float colour[4] = {0.0f, 0.0f, 0.0f, 0.0f};
};

// what the adapter reads of one unit in view
struct BarUnitFacts {
	int32_t id = -1;
	bool mine = false;                 // the viewer's own team
	bool allied = false;               // the viewer's ally team
	bool seen = false;                 // allied, full view, or an enemy in sight now (never radar or fog)
	bool selected = false, hovered = false;
	float health = 0.0f, maxHealth = 0.0f;
	float buildProgress = 1.0f;
	bool upgrading = false;            // rules param cmd_upgrading
	float upgradeProgress = 0.0f;      // cmd_upgrade_progress
	int veterancy = 0;                 // Core::Veterancy::VetMark, rules param vet_level
	float selfDestructSeconds = -1.0f; // left of a countdown (selfd_frame); below 0: none
	bool hasShield = false;            // shd_max_health and a shield_health value
	float shield = 0.0f;               // 0..1 of it
	bool shieldUp = false;             // shield_up
	bool hasFuel = false;
	float fuel = 0.0f;                 // 0..1
	bool fuelLow = false;
	float screen[3] = {0.0f, 0.0f, 0.0f};   // the point above the unit: view px x, y, depth
	float cameraDistance = 0.0f;
};

enum class HealthBand : uint8_t { Healthy, Damaged, Critical };

struct BarState {
	std::vector<BarInstance> instances;
	std::vector<BarText> texts;
	int drawn = 0;                     // units with bars or marks
	int fuels = 0;                     // fuel bars among them

	void Init(int32_t maxBars);
	void Begin();                      // empty, keeping the memory
};

float HideZoom(const BarConfig& config, float detailFactor);
// false: nothing is drawn (HUD hidden, bars off, or the camera zoom at or beyond hide_zoom)
bool BarsShown(const BarConfig& config, const IconLexicon::HudOptions& options, float cameraZoom, float detailFactor);
float SizeFactor(const BarConfig& config, float distance);
HealthBand BandOf(const BarConfig& config, float fraction);
// R1, R1a, R4: whether the health stack shows: own team always; allied when selected, hovered, damaged, building or
// upgrading; an enemy only with enemy bars on and in sight
bool ShowsBars(const IconLexicon::HudOptions& options, const BarUnitFacts& unit);
// false: the cap is reached and the unit is neither selected nor hovered
bool UnderCap(const BarState& state, const BarConfig& config, const BarUnitFacts& unit);
// appends the unit's rectangles and countdown in the widget's order
void AddUnitBars(BarState& state, const BarConfig& config, const IconLexicon::HudOptions& options, const BarUnitFacts& unit);

} // namespace KT1::Core::UnitBars

#endif
