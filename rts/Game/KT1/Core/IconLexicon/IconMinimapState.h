/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 core: the minimap's unit layer, made from the icon set (LUA-FREE H01, UNITDRAW §4.4; replaces icon_minimap).
// No engine include. Same units, same draw order and colours as the world set (relation mode "minimap" or "everywhere"
// applies); a unit is its atlas silhouette, a radar contact the grey shape of the world icon.
// Check column "instances".

#ifndef KT1_CORE_ICON_MINIMAP_STATE_H
#define KT1_CORE_ICON_MINIMAP_STATE_H

#include <cstdint>
#include <span>
#include <vector>

#include "IconState.h"

namespace KT1::Core::IconLexicon {

// the shader's modes (MinimapInstance::mode)
constexpr int MM_NORMAL = 0, MM_INVERTED = 1, MM_CONTACT = 3;   // DESIGN

// One minimap icon as the GPU reads it: four vec4 attributes at locations 1..4, 64 bytes, no padding.
struct MinimapInstance {
	float cell[4] = {0.0f, 0.0f, 0.0f, 0.0f};     // location 1: atlas x, y, w, h px; a contact: shape, bars, flags, scale
	float colour[4] = {0.0f, 0.0f, 0.0f, 1.0f};   // location 2: rgb, fill alpha
	float mode = 0.0f;                            // location 3
	float size = 0.0f;                            // minimap px; 0: hidden
	float turns = 0.0f;                           // 1: turned to the unit's heading
	float unitSlot = 0.0f;                        // the engine's per-object uniform index
	float at[4] = {0.0f, 0.0f, 0.0f, 0.0f};       // location 4: a contact's minimap px x, y; w 1; w 0: the unit's drawn position
};

static_assert(sizeof(MinimapInstance) == 64, "the minimap icon shader reads four vec4");

struct MinimapState {
	std::vector<int32_t> order;                 // unit ids in draw order
	std::vector<MinimapInstance> instances;     // parallel to order
	std::vector<int32_t> contacts;              // places that are radar contacts, ascending
	std::vector<SlotRange> dirty;
	uint32_t seenChanges = 0;                   // IconState::changes at the last build
	uint32_t frame = 0;                         // drawn frames, for the contact spread
	bool built = false;

	void Init(int32_t maxInstances);
};

// the instance of one unit; false: no atlas cell at this size
bool MakeMinimapInstance(const IconConfig& config, const IconClass* cls, const IconUnitFacts& unit, const Paint& paint,
                         IconKind kind, MinimapInstance& out);
// a world position on a minimap of w x h px, y up from the bottom (world +z is down)
void MinimapPoint(const float (&pos)[3], float mapX, float mapZ, float w, float h, float& outX, float& outY);
void BuildMinimap(MinimapState& state, const IconConfig& config, const IconState& icons, std::span<const IconUnitFacts> facts,
                  std::span<const IconClass> classes, std::span<const Paint> paints);
bool ContactDue(const MinimapState& state, const IconMinimapConfig& config, int32_t unit);   // this frame's share

} // namespace KT1::Core::IconLexicon

#endif
