/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Core/IconLexicon: what a strategic icon is and how big it is drawn (LUA-FREE-WORKPACKAGES D01).
// Replaces: the pure parts of LuaUI/Widgets/icon_lex.lua (Collect, BelowFilter, GpuBox, Paint, FillColour) and
//   LuaUI/lib/icon_shape.lua (Shape, Tier, Mark, Group, Body, Extent, Box).
// Phase: draw (unsynced). Events: none here; the draw adapter (Draw/StrategicIcons) calls these per def at Start and per
//   unit on create, destroy, sight and radar change. Config: IconLexiconConfig (JSON "iconlexicon" once F04 lands).
// Requirements (MANIFEST 2026-11-27): UI-424 v5 (released: size by shape group, selected rim, icon scale step),
//   CAM-003 v5 (released: icons in one step past the switch, no plate, shape names the class, overlap without spreading).
//   The far-view rule "every unit an icon at any zoom" (RND-005) and the detail-scaled switch (RND-006 R6a) are withdrawn:
//   the adapter passes allIcons in as a plain fact and nothing here rests on them.
// No engine include.

#ifndef KT1_CORE_ICON_LEXICON_CORE_H
#define KT1_CORE_ICON_LEXICON_CORE_H

#include <array>
#include <cstdint>

namespace KT1::Core::IconLexicon {

// CAM-003 v5 R6: the shape names the class. The values are the shader's shape codes (icon_shape.lua SHAPE).
enum class Shape : uint8_t { Diamond = 0, Square = 1, Rect = 2, Circle = 3, Triangle = 4 };

// UI-424 v5 R1: the size groups. A shape with no group of its own uses SizeTable::unmeasuredGroup.
enum class Group : uint8_t { LandVehicle = 0, Structure, Factory, Commander, SuperUnit, Count };
inline constexpr int GROUP_COUNT = static_cast<int>(Group::Count);

// The outline family a unit def names (customParams icon_outline), reduced to what decides the shape.
enum class Outline : uint8_t { Land, Square, Circle, Wall, Intercept, Bomber, FighterBomber, Gunship, Transport, Other };

// Per-def icon facts, read once from def data at Start (customParams icon_outline, icon_glyph, icon_tier, icon_cmd, spc_blip).
struct IconFields {
	Outline outline = Outline::Land;
	bool factoryGlyph = false; // icon_glyph facLand, facAir or facSea
	bool hasGlyph = false;     // icon_glyph other than "none"
	bool command = false;      // icon_cmd set
	int tier = 1;              // 1..3, 4 for experimental ("X")
	bool canMove = true;       // a structure (cannot move) seen before keeps its icon as a ghost
	bool falseContact = false; // spc_blip: a false radar contact, never drawn (Classify)
};

// Size table: [w, h] in screen pixels at icon scale one (UI-424 v5 R1). hasExtent false: the extent is open (OPEN-UI424-1)
// and is the body plus the unmeasured group's bar strip.
struct Size2 { float w = 0; float h = 0; };
struct SizeTable {
	std::array<Size2, GROUP_COUNT> body{};
	std::array<Size2, GROUP_COUNT> extent{};
	std::array<bool, GROUP_COUNT> hasExtent{};
	Group unmeasuredGroup = Group::LandVehicle;
	float selectedRimPx = 0;
};

struct IconLexiconConfig {
	SizeTable sizes;
	float wallMinPx = 0;       // a wall's icon is hidden below this box size unless detail is forced (icon_lex GpuBox)
	float detailBelowPx = 0;   // Alt held: a box whose larger side is below this (icon_lex Lod level 2) ...
	float detailMinPx = 0;     // ... is drawn with its larger side at this size (icon_lex Icon)
	float contactGrey = 0;     // radar contact body grey, 0..1 (owner review 2026-11-14)
	float cloakedFillAlpha = 0;
	std::array<float, 3> selfColour{}, allyColour{}, enemyColour{}; // the relation colour mode (icon_lex REL)
	float fillContrast = 0;    // white lines keep this contrast over the fill (icon_lex FillColour)
	float fillDarkenStep = 0;  // each darkening step multiplies the colour by this
	int fillDarkenMaxSteps = 0;
};

// The values the Lua uses today (game/design/ui/icon_sizes.lua and icon_lex.lua), each with its source label.
IconLexiconConfig DefaultConfig();

Shape ShapeOf(const IconFields& f);
int TierBars(const IconFields& f); // UI-424 v5 R7: 1..3 bars; an experimental shows 3
bool HasMark(const IconFields& f); // CAM-003 v5 R6: the small black role mark
Group GroupOf(Shape shape, const SizeTable& t);
Size2 BodyOf(Shape shape, const SizeTable& t);
Size2 ExtentOf(Shape shape, const SizeTable& t);

// The drawn box of one shape at icon scale `scale` (UI-424 v5 R1, R2, R4): the extent plus the selection rim on every side
// (the rim space is empty unless selected). dy: from the box centre down to the body centre, which sits on the unit.
struct Box {
	float w = 0, h = 0, dy = 0;
	float bodyW = 0, bodyH = 0, extentW = 0, extentH = 0;
};
Box IconBox(Shape shape, float scale, const SizeTable& t);

// The box a full icon is drawn at; false when it is hidden (a small wall). detail: Alt held.
bool DrawnBox(const IconFields& f, float scale, bool detail, const IconLexiconConfig& c, Box& out);

// Contact shape: the type's own shape when the type is known, else a triangle in the air and a diamond elsewhere.
Shape ContactShape(const IconFields* known, bool inAir);

// How one unit is drawn this pass.
enum class Kind : uint8_t { None = 0, Full = 1, Contact = 2 };
enum class Relation : uint8_t { Self, Ally, Enemy };

struct UnitSight {
	Relation relation = Relation::Self;
	bool inLos = false;
	bool inRadar = false;
	bool seenBefore = false;   // los bit 4: the player saw it once (a structure stays as a ghost)
};
struct ViewFacts {
	bool allIcons = false;     // past the switch (CAM-003 v5 R1), or the adapter draws every unit as an icon
	bool fullView = false;     // spectator or fog off: every unit as it is
};
// icon_lex Collect and BelowFilter in one rule. known: the unit's def facts, or null when its type is hidden (a radar-only
// enemy). Past the switch every unit the player knows of is an icon; an enemy out of sight that is not a ghost structure is a
// grey contact. Below it only those contacts are drawn, and none with the full view. A known false contact is never drawn.
Kind Classify(const IconFields* known, const UnitSight& s, const ViewFacts& v);

// Shader flags of a full icon (icon_lex GpuRebuild): 1 selected, 2 hovered, 4 role mark.
int IconFlags(bool selected, bool hovered, bool mark);

// A team colour darkened along its own hue until white lines keep c.fillContrast over it (icon_lex FillColour).
std::array<float, 3> FillColour(std::array<float, 3> rgb, const IconLexiconConfig& c);

} // namespace KT1::Core::IconLexicon

#endif
