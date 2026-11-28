/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 FxBuild: how a building site is shown: the ghost model of a unit being built, a placement, a queued site, the
// neighbours it would link with and the builders' reach (replaces LuaUI/lib/site_ghost.lua; BLD-010). The construction
// look uses ActiveSiteStyle; the placement widgets (family INPUT) keep the Lua copy until they are ported and then call
// these. Tints in 0 .. 1, lengths elmos. Engine-free.

#ifndef KT1_SITE_GHOST_H
#define KT1_SITE_GHOST_H

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace KT1::FxBuild {

struct SiteGhostConfig {              // "site_ghost"
	float tint_valid[3] = {0.0f, 0.847059f, 0.0f};    // PARITY: c BVI-01 (G0-BLDVIS valid_active_rgb)
	float tint_note[3] = {0.95f, 0.75f, 0.20f};       // PLACEHOLDER: valid with a note
	float tint_refused[3] = {0.847059f, 0.0f, 0.0f};  // PARITY: c BVI-01 (G0-BLDVIS blocked_active_rgb)
	float tint_queued[3] = {0.0f, 0.501961f, 1.0f};   // PARITY: c BVI-01 (G0-BLDVIS queued_site_rgb)
	float skirt_alpha = 0.30f;        // DESIGN
	float queued_model_alpha = 0.35f; // DESIGN
	float idle_model_alpha = 0.25f;   // DESIGN
	float idle_scale = 0.6f;          // DESIGN
	float model_alpha = 0.60f;        // DESIGN
	float ghost_from = 0.45f;         // DESIGN: the model's alpha when construction starts
	float fill_alpha = 0.0f;          // DESIGN
	float line_alpha = 0.90f;         // DESIGN
	float line_width = 1.5f;          // DESIGN
	float lift = 3.0f;                // DESIGN
	float neighbour_radius = 256.0f;  // DESIGN
	int neighbour_max = 48;           // DESIGN
	float neighbour_rgb[3] = {0.85f, 0.85f, 0.80f};   // DESIGN
	float neighbour_alpha = 0.35f;    // DESIGN
	float linked_alpha = 0.95f;       // DESIGN
	float reach_rgb[3] = {0.55f, 0.95f, 0.55f};   // DESIGN
	float reach_alpha = 0.45f;        // DESIGN
	int reach_segments = 64;          // DESIGN
	std::string move_note = "builder must move";  // DESIGN
	float square = 8.0f;              // DESIGN: elmos per footprint square
	template<class S> void Describe(S& s) {
		s.Field("tint_valid", tint_valid);
		s.Field("tint_note", tint_note);
		s.Field("tint_refused", tint_refused);
		s.Field("tint_queued", tint_queued);
		s.Field("skirt_alpha", skirt_alpha);
		s.Field("queued_model_alpha", queued_model_alpha);
		s.Field("idle_model_alpha", idle_model_alpha);
		s.Field("idle_scale", idle_scale);
		s.Field("model_alpha", model_alpha);
		s.Field("ghost_from", ghost_from);
		s.Field("fill_alpha", fill_alpha);
		s.Field("line_alpha", line_alpha);
		s.Field("line_width", line_width);
		s.Field("lift", lift);
		s.Field("neighbour_radius", neighbour_radius);
		s.Field("neighbour_max", neighbour_max);
		s.Field("neighbour_rgb", neighbour_rgb);
		s.Field("neighbour_alpha", neighbour_alpha);
		s.Field("linked_alpha", linked_alpha);
		s.Field("reach_rgb", reach_rgb);
		s.Field("reach_alpha", reach_alpha);
		s.Field("reach_segments", reach_segments);
		s.Field("move_note", move_note);
		s.Field("square", square);
	}
};

enum class SiteState : uint8_t { Valid, Refused, Queued, QueuedInvalid, Active };

struct SiteStyle {
	SiteState state = SiteState::Valid;
	float rgb[3] = {1.0f, 1.0f, 1.0f};
	float model = 0.0f, fill = 0.0f, line = 0.0f, width = 0.0f;
};

struct SiteAt { int32_t defId = -1, facing = 0; float x = 0.0f, y = 0.0f, z = 0.0f; bool refused = false; };
struct Structure { int32_t unit = -1; float x = 0.0f, z = 0.0f; bool linked = false; };
struct Reach { float x = 0.0f, z = 0.0f, radius = 0.0f; };
struct Segment { float x0 = 0.0f, z0 = 0.0f, x1 = 0.0f, z1 = 0.0f; };
struct QueuedWork { float x = 0.0f, z = 0.0f, work = 0.0f; };

float GhostAlpha(const SiteGhostConfig& c, float progress);
// footprint sides in elmos from the def's squares, turned by the facing
void Footprint(const SiteGhostConfig& c, int32_t xsize, int32_t zsize, int32_t facing, float& width, float& depth);
SiteStyle ActiveStyle(const SiteGhostConfig& c, bool refused);
SiteStyle QueuedStyle(const SiteGhostConfig& c, bool valid, bool bright);
SiteStyle ActiveSiteStyle(const SiteGhostConfig& c, float progress);
// seconds until each queued site starts; speed and power 0: not counted
void StartTimes(float x, float z, float speed, float power, std::span<const QueuedWork> queue, std::vector<float>& out);
std::string StartLabel(float seconds);
// the structures outlined round the sites: linked ones first, then the nearest, at most neighbour_max; ids ascending
void Neighbours(const SiteGhostConfig& c, std::span<const SiteAt> sites, std::span<const Structure> structures,
	std::vector<int32_t>& out);
bool InReachOfAny(float x, float z, float width, float depth, std::span<const Reach> builders);   // no builder: true
// the parts of the reach circles outside every other circle
void ReachArcs(const SiteGhostConfig& c, std::span<const Reach> circles, std::vector<Segment>& out);

} // namespace KT1::FxBuild

#endif
