/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 core: icon markup for the Lua HUD (see IconHud.h). A port of icon_lex's Badges, HudCell and HudRml: the same
// markup byte for byte, so the HUD's documents do not change.

#include "IconHud.h"

#include <cstdarg>
#include <cstdio>

namespace KT1::Core::IconLexicon {

namespace {

constexpr float HALF = 0.5f;           // DESIGN: a centre is half a size in
constexpr int BADGE_LOD = 2;           // DESIGN (icon_lex): badges from this detail level
constexpr int BADGES_SMALL = 2;        // DESIGN: at the first badge level at most this many
constexpr int LINE = 512;              // DESIGN: room for one tag
constexpr float BYTE = 255.0f;         // DESIGN: a colour channel's full value

const char* const DIR = "../../LuaUI/icons/";   // DESIGN: from a document two folders deep

#if defined(__GNUC__)
__attribute__((format(printf, 2, 3)))   // DESIGN: the format and the first argument it takes
#endif
void Add(std::string& out, const char* format, ...) {
	char text[LINE];
	va_list args;
	va_start(args, format);
	const int n = std::vsnprintf(text, sizeof(text), format, args);
	va_end(args);
	if (n > 0) out.append(text, static_cast<size_t>(n) < sizeof(text) ? static_cast<size_t>(n) : sizeof(text) - 1);
}

// one sub-rectangle of a HUD layer atlas, placed at (left, top) dp and scaled by k
void Image(std::string& out, const char* file, const float (&rect)[4], float left, float top, float k, const std::string& extra) {
	Add(out, "<img src=\"%s%s\" rect=\"%d %d %d %d\" style=\"position: absolute; left: %.2fdp; top: %.2fdp; "
	         "width: %.2fdp; height: %.2fdp;", DIR, file, static_cast<int>(rect[0]), static_cast<int>(rect[1]),
	    static_cast<int>(rect[2]), static_cast<int>(rect[3]), left, top, rect[2] * k, rect[3] * k);
	out += extra;
	out += "\"/>";
}

const float* CellOf(const IconAtlasEntry& e, int level) {
	switch (level) {
		case 0: return e.l0;
		case 1: return e.l1;
		case 2: return e.l2;   // DESIGN: the atlas's detail levels
		case 3: return e.l3;   // DESIGN
		default: return e.l4;
	}
}

const char* NameOf(Condition condition) {
	switch (condition) {
		case Condition::Critical: return "crit";
		case Condition::Damaged: return "dmg";
		case Condition::Fuel: return "fuel";
		default: return nullptr;
	}
}

const char* NameOf(Activity activity) {
	switch (activity) {
		case Activity::Build: return "build";
		case Activity::Idle: return "idle";
		default: return nullptr;
	}
}

// the badges beside the icon, in the order condition, activity, veterancy, cloak
void AddBadges(std::string& out, const IconAtlasConfig& atlas, const Badges& badges, int level, float k) {
	const char* const slots[] = {NameOf(badges.condition), NameOf(badges.activity), badges.veterancy > 0 ? "vet" : nullptr,
	                             badges.cloaked ? "st" : nullptr};
	int shown = 0;
	for (const char* name: slots) {
		if (name == nullptr || (level <= BADGE_LOD && shown >= BADGES_SMALL)) continue;
		++shown;
		const std::string key = std::string("b|") + name + "|" + std::to_string(level);
		for (const IconBadgeEntry& e: atlas.badges) {
			if (e.key != key) continue;
			const float rect[4] = {e.cell[0], e.cell[1], e.cell[2], e.cell[3]};   // DESIGN: x, y, w, h
			Image(out, "hud_badges.png", rect, e.cell[4] * k, e.cell[5] * k, k, "");   // DESIGN: then the offset
			break;
		}
	}
}

} // namespace

Badges BadgesFor(const IconStyleConfig& style, const IconClass& cls, const IconUnitFacts& unit) {
	Badges badges;
	if (unit.maxHealth > 0.0f) {
		const float share = unit.health / unit.maxHealth;
		if (share < style.crit_health) badges.condition = Condition::Critical;
		else if (share < style.low_health) badges.condition = Condition::Damaged;
	}
	const bool own = unit.relation == Relation::Self;
	if (own && cls.domain == Domain::Air && badges.condition == Condition::None && unit.fuelLow) badges.condition = Condition::Fuel;
	if (own || unit.relation == Relation::Ally) {
		if (unit.building) badges.activity = Activity::Build;
		else if (cls.mobile && unit.idle) badges.activity = Activity::Idle;
		badges.cloaked = unit.cloaked;
	}
	if (unit.veterancy > 0) badges.veterancy = unit.veterancy;
	return badges;
}

std::string HudRml(const IconConfig& config, const IconClass& cls, int px, const Paint& paint, const Badges& badges) {
	const IconAtlasEntry* entry = nullptr;
	for (const IconAtlasEntry& e: config.atlas.icons) {
		if (e.key == cls.key) {
			entry = &e;
			break;
		}
	}
	if (entry == nullptr) return {};
	const int level = Lod(static_cast<float>(px), config.style);
	const float k = static_cast<float>(px) / config.atlas.bake[level];
	const float* cell = CellOf(*entry, level);
	const float rect[4] = {cell[0], cell[1], cell[2], cell[2]};   // DESIGN: x, y and the square cell's side twice
	const float off = (static_cast<float>(px) - cell[2] * k) * HALF;   // DESIGN: the cell's third number is its side
	std::string fill;
	Add(fill, " image-color: rgba(%d, %d, %d, 255);", static_cast<int>(paint.colour.r * BYTE),
	    static_cast<int>(paint.colour.g * BYTE), static_cast<int>(paint.colour.b * BYTE));
	if (badges.cloaked) Add(fill, " opacity: %.2f;", config.style.cloak_alpha);
	std::string out;
	Add(out, "<div class=\"lexico\" style=\"position: relative; width: %ddp; height: %ddp;\">", px, px);
	Image(out, "hud3_fill.png", rect, off, off, k, fill);
	Image(out, paint.inverted ? "hud3_line_inv.png" : "hud3_line.png", rect, off, off, k, "");
	if (level >= BADGE_LOD) AddBadges(out, config.atlas, badges, level, k);
	out += "</div>";
	return out;
}

} // namespace KT1::Core::IconLexicon
