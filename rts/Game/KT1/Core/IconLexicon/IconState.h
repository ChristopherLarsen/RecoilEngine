/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 core: the icon instance set and its rules (LUA-FREE H01, UNITDRAW §4.3-4.4; replaces the strategic pass of
// icon_lex). No engine include. The adapter fills IconUnitFacts for the units an event named (and for all of them on a
// rebuild), Core decides what each looks like and keeps the instances; both draws read the same set.
//
// Draw order is creation order, later on top (ICON reference answer 8), ties by id: `order` and `instances` run in that
// order. The set changes on unit create, destroy, sight and radar change, selection, hover, cloak, options and level
// switch, never per frame, except radar contacts (their position moves every frame).
// Check column "instances": position, class, colour and state bits, by ascending unit id.

#ifndef KT1_CORE_ICON_STATE_H
#define KT1_CORE_ICON_STATE_H

#include <cstdint>
#include <span>
#include <vector>

#include "Game/KT1/Core/Common/Column.h"
#include "Game/KT1/Core/Common/DenseIndex.h"
#include "Game/KT1/Core/DrawHost/InstanceStore.h"
#include "HudOptions.h"
#include "IconClass.h"

namespace KT1::Core::IconLexicon {

enum class Relation : uint8_t { Self, Ally, Enemy };
enum class IconKind : uint8_t { None, Full, Contact };
enum class ContactLayer : uint8_t { Land, Air, Naval, Sub };

struct Rgb {
	float r = 0.0f, g = 0.0f, b = 0.0f;
};

// what the adapter reads of one unit
struct IconUnitFacts {
	int32_t id = -1;
	int32_t defId = 0;                 // an index into the classes; below 0: the type is not known
	int32_t team = 0;
	int32_t unitSlot = 0;              // its per-object uniform index
	Relation relation = Relation::Enemy;
	bool fullView = false;             // spectator or full local view: no contacts, no ghosts
	bool inLos = false, inRadar = false;
	bool seenBefore = false;           // sight once had (the previous-LOS bit): a structure keeps a ghost icon
	bool selected = false;
	bool cloaked = false;
	bool inView = false;               // in the camera's view (needed below the switch)
	float health = 0.0f, maxHealth = 0.0f;
	float pos[3] = {0.0f, 0.0f, 0.0f}; // as the viewer sees it (a radar contact's carries the radar error)
	float groundHeight = 0.0f;         // under pos (contacts)
	// badges (HUD markup)
	bool building = false, idle = false, fuelLow = false;
	int veterancy = 0;
};

struct IconParams {
	float scale = 1.0f;                // NearestStep of the option
	bool alt = false;
	bool pastSwitch = false;           // the camera is at or beyond the switch (or the far view forces icons)
	int32_t hovered = -1;
};

// section 8 colours: the fill of a team in a context, and whether it is the inverted enemy
struct Paint {
	Rgb colour;
	bool inverted = false;
};
Rgb FillColour(const Rgb& team, const IconStyleConfig& style);   // darkened until white lines keep their contrast
Paint PaintFor(const IconConfig& config, RelationMode mode, bool minimap, Relation relation, const Rgb& teamFill);

// what a unit shows: past the switch every unit the viewer may see; below it radar-only enemies (not ghosts) alone;
// a known fake never, a hidden one as any contact
IconKind KindOf(const IconClass* cls, const IconUnitFacts& unit, bool pastSwitch);
ContactLayer LayerOf(float height, float groundHeight, const IconViewConfig& view);

// the instance of a unit that shows an icon; false: hidden (a wall below the size limit). cls may be null for a contact
// of unknown type.
bool MakeInstance(const IconConfig& config, const IconParams& params, const IconClass* cls, const IconUnitFacts& unit,
                  const Paint& paint, IconKind kind, IconInstance& out);

// the box of a drawn icon on screen, view px, y up
struct IconRect {
	int32_t unit = -1;
	float x = 0.0f, y = 0.0f;          // centre
	float w = 0.0f, h = 0.0f;
};

int32_t HitRect(std::span<const IconRect> rects, float x, float y);   // the topmost (last) box holding the point, -1
bool IsDoubleClick(float seconds, float dx, float dy, bool sameDef, float gap, float px);

struct IconState {
	DenseIndex units;
	Column<uint32_t> born;             // creation or first-sight counter; 0: not yet
	Column<int32_t> defId, team;
	Column<uint16_t> bits;             // ICON_ bits
	Column<uint8_t> kind;              // IconKind as drawn
	Column<int32_t> place;             // index into order; -1: no instance
	Column<float> flickerUntil;        // draw seconds; 0: not flickering
	Column<Rgb> ownColour;             // the instance's colour without the flicker
	std::vector<int32_t> lit;          // the units whose flicker window is open
	int32_t limit = 0;                 // instances at most

	std::vector<int32_t> order;              // unit ids in draw order
	std::vector<IconInstance> instances;     // parallel to order
	std::vector<int32_t> contacts;           // places that are radar contacts, ascending
	std::vector<int32_t> pending;            // units an event named since the last apply
	std::vector<SlotRange> dirty;            // instance ranges the GPU lacks
	uint32_t bornCount = 0;
	uint32_t changes = 0;                    // moves when order or instances change
	bool rebuild = true;                     // the whole set is to be made again

	void Init(int32_t maxIds, int32_t maxInstances);
};

void IconCreated(IconState& state, int32_t id);     // takes a creation number
void IconSeen(IconState& state, int32_t id);        // one at first sight, when it has none
void IconGone(IconState& state, int32_t id);        // its instance leaves at once, its creation number is forgotten
void IconMark(IconState& state, int32_t id);        // rewrite this unit at the next apply
void IconMarkAll(IconState& state);                 // selection, players, options, switch: rebuild
// makes the whole set from the facts of every candidate unit
void RebuildIcons(IconState& state, const IconConfig& config, const IconParams& params,
                  std::span<const IconUnitFacts> facts, std::span<const IconClass> classes, std::span<const Paint> paints);
// rewrites one pending unit in place; false: it has to be inserted inside the order, so a rebuild is due
bool ApplyIcon(IconState& state, const IconConfig& config, const IconParams& params, const IconUnitFacts& unit,
               const IconClass* cls, const Paint& paint);
void FlickerStart(IconState& state, const IconViewConfig& view, int32_t id, float seconds);
void FlickerStep(IconState& state, const IconViewConfig& view, float seconds);   // writes and restores colours

} // namespace KT1::Core::IconLexicon

#endif
