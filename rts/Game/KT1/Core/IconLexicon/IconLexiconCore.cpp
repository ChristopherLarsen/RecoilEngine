/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Core/IconLexicon: icon shape, size, kind and fill colour. See IconLexiconCore.h and docs/engine/rules/iconlexicon.md.

#include "IconLexiconCore.h"

#include <algorithm>
#include <cmath>

namespace KT1::Core::IconLexicon {

namespace {
	constexpr int G(Group g) { return static_cast<int>(g); }

	// sRGB to linear and relative luminance (WCAG 2), as icon_lex Lin and Lum
	float Linear(float c) {
		constexpr float KNEE = 0.04045f, SLOPE = 12.92f, OFFSET = 0.055f, GAMMA = 2.4f; // DESIGN: the sRGB transfer function (IEC 61966-2-1), as icon_lex Lin
		return c <= KNEE ? c / SLOPE : std::pow((c + OFFSET) / (1 + OFFSET), GAMMA);
	}
	float Luminance(const std::array<float, 3>& c) {
		constexpr float WR = 0.2126f, WG = 0.7152f, WB = 0.0722f; // DESIGN: Rec. 709 luminance weights, as icon_lex Lum
		return WR * Linear(c[0]) + WG * Linear(c[1]) + WB * Linear(c[2]);
	}
	// contrast of white over a colour of luminance l (WCAG 2: (L1 + 0.05) / (L2 + 0.05), white L1 = 1)
	float WhiteContrast(float l) {
		constexpr float FLARE = 0.05f; // DESIGN: the WCAG 2 contrast flare term, as icon_lex FillColour
		return (1 + FLARE) / (l + FLARE);
	}
}

IconLexiconConfig DefaultConfig() {
	IconLexiconConfig c;
	SizeTable& t = c.sizes;
	t.body[G(Group::LandVehicle)] = {11, 11}; // DESIGN (game/design/ui/icon_sizes.lua; owner icon review 2026-11-13)
	t.body[G(Group::Structure)] = {9, 9};     // DESIGN (slightly smaller than land, UI-424 v5 R1)
	t.body[G(Group::Factory)] = {13, 7};      // DESIGN (wide rectangle)
	t.body[G(Group::Commander)] = {10, 10};   // DESIGN (round, not larger than land, UI-424 v5 R1)
	t.body[G(Group::SuperUnit)] = {10, 10};   // DESIGN
	t.extent[G(Group::LandVehicle)] = {16, 16}; // DESIGN (about 16 px with the tier bars)
	t.hasExtent[G(Group::LandVehicle)] = true;
	t.unmeasuredGroup = Group::LandVehicle;   // DESIGN (UI-424 v5 R1: proposed design for unmeasured shapes)
	t.selectedRimPx = 1;                      // DESIGN (icon_sizes.lua look.selected_rim_px)
	c.wallMinPx = 20;                         // DESIGN (icon_lex GpuBox)
	c.detailBelowPx = 18;                     // DESIGN (icon_lex Lod: level 2 from 18 px)
	c.detailMinPx = 20;                       // DESIGN (icon_lex Icon: Alt gives at least 20 px detail)
	c.contactGrey = 128.0f / 255;             // DESIGN (owner review 2026-11-14: contact body about RGB 128)
	c.cloakedFillAlpha = 0.35f;               // DESIGN (icon_lex Icon)
	c.selfColour = {0x2f / 255.0f, 0x7f / 255.0f, 0xe0 / 255.0f};  // DESIGN (icon spec section 8)
	c.allyColour = {0x13 / 255.0f, 0x9c / 255.0f, 0x8a / 255.0f};  // DESIGN
	c.enemyColour = {0xff / 255.0f, 0x9a / 255.0f, 0x76 / 255.0f}; // DESIGN
	c.fillContrast = 3;                       // DESIGN (icon spec section 8: white lines keep 3:1)
	c.fillDarkenStep = 0.95f;                 // DESIGN (icon_lex FillColour)
	c.fillDarkenMaxSteps = 40;                // DESIGN (icon_lex FillColour)
	return c;
}

Shape ShapeOf(const IconFields& f) {
	constexpr int EXPERIMENTAL = 4; // DESIGN: icon_tier "X" is read as tier 4 (icon_lex Fields)
	if (f.command || f.tier >= EXPERIMENTAL)
		return Shape::Circle;
	if (f.factoryGlyph)
		return Shape::Rect;
	switch (f.outline) {
		case Outline::Square: case Outline::Circle: case Outline::Wall:
			return Shape::Square;
		case Outline::Intercept: case Outline::Bomber: case Outline::FighterBomber: case Outline::Gunship: case Outline::Transport:
			return Shape::Triangle;
		default:
			return Shape::Diamond;
	}
}

int TierBars(const IconFields& f) {
	constexpr int MAX_BARS = 3; // REQ UI-424 v5 R7: one, two or three bars; an experimental shows three
	return std::clamp(f.tier, 1, MAX_BARS);
}

bool HasMark(const IconFields& f) { return f.hasGlyph || f.command; }

Group GroupOf(Shape shape, const SizeTable& t) {
	switch (shape) {
		case Shape::Diamond: return Group::LandVehicle;
		case Shape::Square: return Group::Structure;
		case Shape::Rect: return Group::Factory;
		case Shape::Circle: return Group::Commander;
		default: return t.unmeasuredGroup; // OPEN-UI424-1: no measured size
	}
}

Size2 BodyOf(Shape shape, const SizeTable& t) { return t.body[G(GroupOf(shape, t))]; }

Size2 ExtentOf(Shape shape, const SizeTable& t) {
	const Group g = GroupOf(shape, t);
	if (t.hasExtent[G(g)])
		return t.extent[G(g)];
	// OPEN-UI424-1: the body plus the unmeasured group's bar strip (its extent minus its body)
	const Size2 b = t.body[G(g)];
	const Size2 ub = t.body[G(t.unmeasuredGroup)];
	const Size2 ue = t.hasExtent[G(t.unmeasuredGroup)] ? t.extent[G(t.unmeasuredGroup)] : ub;
	return {b.w + (ue.w - ub.w), b.h + (ue.h - ub.h)};
}

Box IconBox(Shape shape, float scale, const SizeTable& t) {
	const Size2 b = BodyOf(shape, t), e = ExtentOf(shape, t);
	Box x;
	x.bodyW = b.w * scale;
	x.bodyH = b.h * scale;
	x.extentW = e.w * scale;
	x.extentH = e.h * scale;
	const float rim = t.selectedRimPx * scale; // UI-424 v5 R4: the rim scales with the icon
	constexpr float SIDES = 2; // REQ UI-424 v5 R2: the rim is on every side
	constexpr float HALF = 0.5f; // DESIGN: a centre is half a size in
	x.w = x.extentW + SIDES * rim;
	x.h = x.extentH + SIDES * rim;
	// the body's top is one rim below the box top; the bars hang below the body (UI-424 v5 R7)
	x.dy = x.h * HALF - (rim + x.bodyH * HALF);
	return x;
}

bool DrawnBox(const IconFields& f, float scale, bool detail, const IconLexiconConfig& c, Box& out) {
	out = IconBox(ShapeOf(f), scale, c.sizes);
	const float px = std::max(out.w, out.h);
	if (f.outline == Outline::Wall && px < c.wallMinPx && !detail)
		return false;
	if (detail && px < c.detailBelowPx) {
		const float k = c.detailMinPx / px;
		out.w *= k;
		out.h *= k;
	}
	return true;
}

Shape ContactShape(const IconFields* known, bool inAir) {
	if (known != nullptr)
		return ShapeOf(*known);
	return inAir ? Shape::Triangle : Shape::Diamond;
}

Kind Classify(const IconFields* known, const UnitSight& s, const ViewFacts& v) {
	if (known != nullptr && known->falseContact)
		return Kind::None;
	if (v.fullView || s.relation != Relation::Enemy)
		return v.allIcons ? Kind::Full : Kind::None;
	const bool ghost = s.seenBefore && known != nullptr && !known->canMove;
	if (s.inLos)
		return v.allIcons ? Kind::Full : Kind::None;
	if (ghost)
		return v.allIcons ? Kind::Full : Kind::None; // a structure seen before keeps its team-coloured icon (owner 2026-11-14)
	if (!s.inRadar)
		return Kind::None;                           // the player knows nothing of it
	return Kind::Contact;
}

int IconFlags(bool selected, bool hovered, bool mark) {
	constexpr int SELECTED = 1, HOVERED = 2, MARK = 4; // DESIGN: the shader's flag bits (icon_lex VS_GPU)
	return (selected ? SELECTED : 0) | (hovered ? HOVERED : 0) | (mark ? MARK : 0);
}

std::array<float, 3> FillColour(std::array<float, 3> rgb, const IconLexiconConfig& c) {
	for (int i = 0; i < c.fillDarkenMaxSteps; ++i) {
		if (WhiteContrast(Luminance(rgb)) >= c.fillContrast)
			break;
		for (float& x: rgb)
			x *= c.fillDarkenStep;
	}
	return rgb;
}

} // namespace KT1::Core::IconLexicon
