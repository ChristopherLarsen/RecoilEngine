/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 core: icon classes (see IconClass.h). A port of what icon_lex's Fields, LuaUI/lib/icon_shape.lua, icon_size.lua
// and gamedata/icon_map.lua compute, in their order of operations, so the numbers are the Lua's.

#include "IconClass.h"

#include <algorithm>
#include <charconv>
#include <cmath>

namespace KT1::Core::IconLexicon {

namespace {

constexpr float HALF = 0.5f;       // DESIGN: a centre is half a size in
constexpr float SIDES = 2.0f;      // REQ UI-424 v5 R2: the rim is on every side
constexpr int MAX_BARS = 3;        // REQ UI-424 v5 R7: one, two or three bars; an experimental shows three

// icon_tier: "X" is experimental, a number is itself, anything else is 1 (icon_lex Fields)
int TierOf(std::string_view text) {
	if (text == "X") return TIER_EXPERIMENTAL;
	int tier = 1;
	const auto parsed = std::from_chars(text.data(), text.data() + text.size(), tier);
	return (parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size()) ? tier : 1;
}

std::string_view Or(std::string_view value, std::string_view fallback) { return value.empty() ? fallback : value; }

bool IsDirectFire(std::string_view glyph) { return glyph == "guns" || glyph == "gunsIn"; }

Domain DomainOf(std::string_view outline) {
	if (outline == "square" || outline == "circle" || outline == "wall") return Domain::Build;
	if (outline == "ship" || outline == "sub" || outline == "carrier") return Domain::Sea;
	if (outline == "land" || outline == "hover" || outline == "amph") return Domain::Land;
	return Domain::Air;
}

// icon_map.lua Size: the multiple of the base size the HUD contexts use
float SizeOf(std::string_view command, Domain domain, int tier) {
	if (command == "cmd") return 1.45f;       // DESIGN (ICON-02 section 5)
	if (command == "support") return 1.25f;   // DESIGN (ICON-02 section 5)
	const float base = domain == Domain::Sea ? 1.1f : 1.0f;   // DESIGN: sea outlines one step up
	return base + 0.1f * static_cast<float>(tier - 1);        // DESIGN: a step per tier
}

uint8_t DrawGroupOf(const IconDefFacts& def, std::string_view outline, Domain domain) {
	if (!def.command.empty() || def.tier == "X") return 4;    // DESIGN (ICON-02 section 9): the draw groups
	if (outline == "wall") return 0;
	if (domain == Domain::Build) return 1;
	return domain == Domain::Air ? 3 : 2;
}

bool InAtlas(const IconAtlasConfig& atlas, std::string_view key) {
	return std::any_of(atlas.icons.begin(), atlas.icons.end(), [key](const IconAtlasEntry& e) { return e.key == key; });
}

const IconGroupSize* FindGroup(const IconSizeConfig& sizes, std::string_view name) {
	for (const IconGroupSize& g: sizes.groups) {
		if (g.group == name) return &g;
	}
	return nullptr;
}

// the size group of a shape; a shape with no measured size uses the unmeasured group (UI-424 v5 R1)
std::string_view GroupName(Shape shape, const IconSizeConfig& sizes) {
	switch (shape) {
		case Shape::Diamond: return "land_vehicle";
		case Shape::Square: return "structure";
		case Shape::Rect: return "factory";
		case Shape::Circle: return "commander";
		default: return sizes.unmeasured;
	}
}

} // namespace

Shape ShapeFor(std::string_view outline, std::string_view glyph, int tier, bool command) {
	if (command || tier == TIER_EXPERIMENTAL) return Shape::Circle;
	if (glyph == "facLand" || glyph == "facAir" || glyph == "facSea") return Shape::Rect;
	const std::string_view o = Or(outline, "land");
	if (o == "square" || o == "circle" || o == "wall") return Shape::Square;
	if (o == "intercept" || o == "bomber" || o == "fbomber" || o == "gunship" || o == "transport") return Shape::Triangle;
	return Shape::Diamond;
}

std::string MapKey(const IconDefFacts& def) {
	const int tier = TierOf(def.tier);
	const std::string_view glyph = Or(def.glyph, "none");
	std::string shape(Or(def.outline, "land"));
	if (shape == "sub") shape = tier == 1 ? "sub1" : tier == 2 ? "sub2" : "sub3";
	const int bars = IsDirectFire(glyph) ? std::min(tier, MAX_BARS) : 0;
	return shape + "|" + std::string(glyph) + "|" + std::to_string(bars) + "|" + (tier == TIER_EXPERIMENTAL ? "1" : "0");
}

IconClass ClassFor(const IconDefFacts& def, const IconConfig& config) {
	const int tier = TierOf(def.tier);
	const std::string_view outline = Or(def.outline, "land");
	const std::string_view glyph = Or(def.glyph, "none");
	IconClass cls;
	cls.key = std::string(def.key);
	if (cls.key.empty() || !InAtlas(config.atlas, cls.key)) cls.key = MapKey(def);
	if (!InAtlas(config.atlas, cls.key)) cls.key = config.atlas.fallback_key;
	cls.command = !def.command.empty();
	cls.domain = DomainOf(outline);
	cls.shape = ShapeFor(outline, glyph, tier, cls.command);
	cls.tier = static_cast<uint8_t>(std::clamp(tier, 0, 255));
	cls.bars = static_cast<uint8_t>(std::clamp(tier, 1, MAX_BARS));
	cls.drawGroup = DrawGroupOf(def, outline, cls.domain);
	cls.mark = glyph != "none" || cls.command;
	cls.wall = outline == "wall";
	cls.mobile = cls.domain != Domain::Build;
	cls.rotates = config.sizes.rotate && cls.mobile;
	cls.canMove = def.canMove;
	cls.fake = def.fake;
	cls.size = def.size >= 0.0f ? def.size : SizeOf(def.command, cls.domain, tier);
	return cls;
}

int Lod(float px, const IconStyleConfig& style) {
	int level = 0;
	for (const float from: style.lod_px) {
		if (px < from) break;
		++level;
	}
	return level;
}

float NearestStep(const IconSizeConfig& sizes, float value) {
	constexpr float TIE = 1e-9f;   // DESIGN: the Lua's tie tolerance (icon_size.lua Nearest)
	float best = sizes.steps[0];
	if (std::isnan(value)) return best;
	float distance = INFINITY;
	for (const float step: sizes.steps) {
		const float d = std::abs(step - value);
		if (d < distance - TIE || (std::abs(d - distance) <= TIE && step > best)) {
			best = step;
			distance = d;
		}
	}
	return best;
}

IconBox BoxFor(Shape shape, float scale, const IconConfig& config) {
	const IconSizeConfig& sizes = config.sizes;
	const IconGroupSize* unmeasured = FindGroup(sizes, sizes.unmeasured);
	const IconGroupSize* group = FindGroup(sizes, GroupName(shape, sizes));
	if (group == nullptr) group = unmeasured;
	if (group == nullptr) return {};
	float extent[2] = {group->extent[0], group->extent[1]};
	if (extent[0] <= 0.0f && extent[1] <= 0.0f) {
		// no measured extent (OPEN-UI424-1): the body plus the unmeasured group's bar strip
		for (int axis = 0; axis < 2; ++axis) {
			float strip = 0.0f;
			if (unmeasured != nullptr && unmeasured->extent[axis] > 0.0f) strip = unmeasured->extent[axis] - unmeasured->body[axis];
			extent[axis] = group->body[axis] + strip;
		}
	}
	const float bodyH = group->body[1] * scale;
	const float rim = sizes.selected_rim_px * scale;   // UI-424 v5 R4: the rim scales with the icon
	IconBox box;
	box.w = extent[0] * scale + SIDES * rim;
	box.h = extent[1] * scale + SIDES * rim;
	box.lift = box.h * HALF - (rim + bodyH * HALF);    // the body's top is one rim below the box top; bars hang below
	return box;
}

} // namespace KT1::Core::IconLexicon
