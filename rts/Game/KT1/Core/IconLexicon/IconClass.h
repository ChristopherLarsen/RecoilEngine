/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 core: the icon class of a unit def (shape, size rule, tier bars), computed once at Start, and the GPU instance of
// one icon (LUA-FREE H01, UNITDRAW §4.3; replaces icon_lex's Fields, LuaUI/lib/icon_shape.lua, icon_size.lua and the
// drawing half of gamedata/icon_map.lua). No engine include.
// The class tokens come from the def's custom parameters, which gamedata writes at def load (icon_key, icon_outline,
// icon_glyph, icon_tier, icon_cmd, icon_size, icon_walker, spc_blip).

#ifndef KT1_CORE_ICON_CLASS_H
#define KT1_CORE_ICON_CLASS_H

#include <cstdint>
#include <string>
#include <string_view>

#include "IconConfig.h"

namespace KT1::Core::IconLexicon {

// the values are the icon shader's shape numbers
enum class Shape : uint8_t { Diamond = 0, Square = 1, Rect = 2, Circle = 3, Triangle = 4 };
enum class Domain : uint8_t { Land, Sea, Air, Build };

constexpr int TIER_EXPERIMENTAL = 4;   // DESIGN: the tier "X"

struct IconDefFacts {
	std::string_view key;              // custom parameter icon_key
	std::string_view outline;          // icon_outline; empty: land
	std::string_view glyph;            // icon_glyph; empty: none
	std::string_view tier;             // icon_tier: "1".."3" or "X"; empty: 1
	std::string_view command;          // icon_cmd; empty: no command unit
	float size = -1.0f;                // icon_size; below 0: from the class
	bool walker = false;
	bool fake = false;                 // spc_blip: a false contact
	bool canMove = false;              // a unit that cannot move leaves a ghost icon once seen
};

struct IconClass {
	std::string key;                   // the atlas key "shape|glyph|n|dbl"
	Shape shape = Shape::Diamond;
	Domain domain = Domain::Land;
	uint8_t tier = 1;                  // 1..4 (4: experimental)
	uint8_t bars = 1;                  // tier bars drawn, 1..3
	uint8_t drawGroup = 2;             // 0 walls, 1 structures, 2 ground and naval, 3 air, 4 experimentals and commanders
	bool mark = false;                 // a role mark is drawn
	bool command = false;
	bool wall = false;
	bool rotates = false;
	bool mobile = false;
	bool canMove = false;
	bool fake = false;
	float size = 1.0f;                 // multiple of the base size (HUD contexts)
};

IconClass ClassFor(const IconDefFacts& def, const IconConfig& config);
std::string MapKey(const IconDefFacts& def);   // the atlas key of a def without a usable icon_key
Shape ShapeFor(std::string_view outline, std::string_view glyph, int tier, bool command);
int Lod(float px, const IconStyleConfig& style);                      // detail level 0..4
float NearestStep(const IconSizeConfig& sizes, float value);          // the scale option's step; a tie goes up

// the drawn box of a shape at scale s: the extent plus the rim on every side
struct IconBox {
	float w = 0.0f, h = 0.0f;
	float lift = 0.0f;                 // px from the box centre down to the body centre's screen point
};
IconBox BoxFor(Shape shape, float scale, const IconConfig& config);

// state bits of an icon (UNITDRAW §4.3)
constexpr uint16_t ICON_SELECTED = 1, ICON_HOVERED = 2, ICON_CONTACT = 4, ICON_DAMAGED = 8, ICON_CLOAKED = 16;   // DESIGN
// the shader's flag bits (IconInstance::flags)
constexpr int SHADE_SELECTED = 1, SHADE_HOVERED = 2, SHADE_MARK = 4;   // DESIGN

// One icon as the GPU reads it: five vec4 attributes at locations 1..5, 80 bytes, no padding. Floats throughout, so the
// colours are bit for bit what the Lua passed. The vertex shader takes the screen point from the engine's per-unit
// uniforms (uni[unitSlot].drawPos), or from `world` for a radar contact (its position carries the radar error).
struct IconInstance {
	float lift = 0.0f;                 // location 1
	float unitSlot = 0.0f;             // the engine's per-object uniform index (a whole number)
	float width = 0.0f, height = 0.0f; // 0 width: hidden
	float shape = 0.0f;                // location 2
	float bars = 0.0f;
	float flags = 0.0f;                // SHADE_ bits
	float scale = 1.0f;
	float colour[4] = {0.0f, 0.0f, 0.0f, 1.0f};   // location 3: rgb, fill alpha
	float world[4] = {0.0f, 0.0f, 0.0f, 0.0f};    // location 4: contact position, w 1; w 0: the unit's drawn position
	float angle = 0.0f;                // location 5: radians counter-clockwise (rotate only)
	float fade = 0.0f;                 // 1 - opacity
	float unused[2] = {0.0f, 0.0f};
};

static_assert(sizeof(IconInstance) == 80, "the icon shader reads five vec4");

} // namespace KT1::Core::IconLexicon

#endif
