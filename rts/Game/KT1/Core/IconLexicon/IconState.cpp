/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 core: the icon instance set (see IconState.h). A port of icon_lex's strategic pass (Collect, GpuRebuild, IncApply,
// IncGone, the hit flicker): which units show what, in which order, and which instances change.

#include "IconState.h"

#include <algorithm>
#include <cmath>

namespace KT1::Core::IconLexicon {

namespace {

constexpr float HALF = 0.5f;              // DESIGN: a centre is half a size in
constexpr int ALT_LOD = 2;                // DESIGN: with Alt a box below this detail level is enlarged (icon_lex Icon)
constexpr uint16_t QUEUED = 0x8000;       // DESIGN: a private bit of `bits`: the unit is in `pending`
constexpr int32_t GONE = -1;              // an order entry whose unit left
constexpr int AXES = 3;                   // DESIGN: x, y, z, and red, green, blue

// the sRGB transfer function and Rec. 709 luminance, as icon_lex Lin and Lum
float Linear(float c) {
	constexpr float KNEE = 0.04045f, SLOPE = 12.92f, OFFSET = 0.055f, GAMMA = 2.4f;   // DESIGN: IEC 61966-2-1
	return c <= KNEE ? c / SLOPE : std::pow((c + OFFSET) / (1.0f + OFFSET), GAMMA);
}

float Luminance(const Rgb& c) {
	constexpr float WR = 0.2126f, WG = 0.7152f, WB = 0.0722f;   // DESIGN: Rec. 709 weights
	return WR * Linear(c.r) + WG * Linear(c.g) + WB * Linear(c.b);
}

void SetColour(IconInstance& instance, const float (&rgb)[3]) {
	for (int i = 0; i < AXES; ++i) instance.colour[i] = rgb[i];
}

void Dirty(IconState& state, int32_t place) {
	if (!state.dirty.empty()) {
		SlotRange& last = state.dirty.back();
		if (place >= last.first && place < last.first + last.count) return;
		if (place == last.first + last.count) {
			++last.count;
			return;
		}
	}
	if (state.dirty.size() == state.dirty.capacity() && !state.dirty.empty()) {   // never consumed: everything is stale
		state.dirty.clear();
		state.dirty.push_back({0, static_cast<int>(state.instances.size())});
		return;
	}
	state.dirty.push_back({place, 1});
}

void SetContact(IconState& state, int32_t place, bool contact) {
	const auto it = std::lower_bound(state.contacts.begin(), state.contacts.end(), place);
	const bool listed = it != state.contacts.end() && *it == place;
	if (contact && !listed) state.contacts.insert(it, place);
	if (!contact && listed) state.contacts.erase(it);
}

uint16_t BitsOf(const IconStyleConfig& style, const IconParams& params, const IconUnitFacts& unit, IconKind kind) {
	uint16_t bits = 0;
	if (unit.selected) bits |= ICON_SELECTED;
	if (unit.id == params.hovered) bits |= ICON_HOVERED;
	if (kind == IconKind::Contact) bits |= ICON_CONTACT;
	if (unit.maxHealth > 0.0f && unit.health / unit.maxHealth < style.low_health) bits |= ICON_DAMAGED;
	if (unit.cloaked) bits |= ICON_CLOAKED;
	return bits;
}

// the unit's slot, made when it has none; NONE for an id out of range
int32_t Slot(IconState& state, int32_t id) {
	const int32_t had = state.units.SlotOf(id);
	return had != DenseIndex::NONE ? had : state.units.Add(id);
}

// writes `instance` as unit `slot`'s own look at `place`, the hit flicker's colour over it while its window is open
void Write(IconState& state, const IconViewConfig* view, int32_t slot, int32_t place, const IconInstance& instance) {
	state.instances[place] = instance;
	state.ownColour[slot] = {instance.colour[0], instance.colour[1], instance.colour[2]};
	const bool full = state.kind[slot] == static_cast<uint8_t>(IconKind::Full);
	if (view != nullptr && full && state.flickerUntil[slot] > 0.0f) SetColour(state.instances[place], view->flicker_colour);
}

void Remember(IconState& state, int32_t slot, const IconUnitFacts& unit, uint16_t bits, IconKind kind) {
	state.defId[slot] = unit.defId;
	state.team[slot] = unit.team;
	state.bits[slot] = bits;
	state.kind[slot] = static_cast<uint8_t>(kind);
}

// the creation number of the last unit in the order
uint32_t NewestBorn(const IconState& state) {
	for (auto it = state.order.rbegin(); it != state.order.rend(); ++it) {
		if (*it != GONE) return state.born[state.units.SlotOf(*it)];
	}
	return 0;
}

} // namespace

Rgb FillColour(const Rgb& team, const IconStyleConfig& style) {
	constexpr float FLARE = 0.05f;   // DESIGN: the WCAG 2 contrast flare term; white has luminance 1
	Rgb c = team;
	for (int i = 0; i < style.team_darken_steps; ++i) {
		if ((1.0f + FLARE) / (Luminance(c) + FLARE) >= style.team_contrast) break;
		c = {c.r * style.team_darken, c.g * style.team_darken, c.b * style.team_darken};
	}
	return c;
}

Paint PaintFor(const IconConfig& config, RelationMode mode, bool minimap, Relation relation, const Rgb& teamFill) {
	if (mode != RelationMode::Everywhere && !(mode == RelationMode::Minimap && minimap)) return {teamFill, false};
	const IconStyleConfig& s = config.style;
	const float* c = relation == Relation::Self ? s.relation_self : relation == Relation::Ally ? s.relation_ally : s.relation_enemy;
	return {{c[0], c[1], c[2]}, relation == Relation::Enemy};
}

IconKind KindOf(const IconClass* cls, const IconUnitFacts& unit, bool pastSwitch) {
	if (cls != nullptr && cls->fake) return IconKind::None;
	const bool unseenEnemy = !unit.fullView && unit.relation == Relation::Enemy && !unit.inLos;
	const bool ghost = unit.seenBefore && cls != nullptr && !cls->canMove;   // a structure seen before keeps its icon
	const bool contact = unseenEnemy && !ghost;
	if (contact) return unit.inRadar ? IconKind::Contact : IconKind::None;
	return (pastSwitch && cls != nullptr) ? IconKind::Full : IconKind::None;
}

ContactLayer LayerOf(float height, float groundHeight, const IconViewConfig& view) {
	if (height > std::max(groundHeight, 0.0f) + view.contact_air_above) return ContactLayer::Air;
	if (height < view.contact_sub_below) return ContactLayer::Sub;
	return groundHeight < 0.0f ? ContactLayer::Naval : ContactLayer::Land;
}

bool MakeInstance(const IconConfig& config, const IconParams& params, const IconClass* cls, const IconUnitFacts& unit,
                  const Paint& paint, IconKind kind, IconInstance& out) {
	if (kind == IconKind::None || (kind == IconKind::Full && cls == nullptr)) return false;
	const bool contact = kind == IconKind::Contact;
	Shape shape = Shape::Diamond;
	if (cls != nullptr) shape = cls->shape;
	else if (LayerOf(unit.pos[1], unit.groundHeight, config.view) == ContactLayer::Air) shape = Shape::Triangle;
	const IconBox box = BoxFor(shape, params.scale, config);
	float w = box.w, h = box.h;
	const float px = std::max(w, h);
	if (!contact && cls->wall && px < config.style.wall_min_px && !params.alt) return false;
	if (params.alt && Lod(px, config.style) < ALT_LOD) {
		w = w * config.style.alt_min_px / px;
		h = h * config.style.alt_min_px / px;
	}
	out = IconInstance{};
	out.lift = box.lift;
	out.unitSlot = static_cast<float>(unit.unitSlot);
	out.width = w;
	out.height = h;
	out.shape = static_cast<float>(shape);
	out.scale = params.scale;
	if (contact) {
		SetColour(out, config.style.contact_grey);
		for (int i = 0; i < AXES; ++i) out.world[i] = unit.pos[i];
		out.world[3] = 1.0f;
		return true;
	}
	out.bars = static_cast<float>(cls->bars);
	out.flags = static_cast<float>((unit.selected ? SHADE_SELECTED : 0) | (unit.id == params.hovered ? SHADE_HOVERED : 0)
	                               | (cls->mark ? SHADE_MARK : 0));
	out.colour[0] = paint.colour.r;
	out.colour[1] = paint.colour.g;
	out.colour[2] = paint.colour.b;
	const bool shownCloaked = unit.cloaked && unit.relation != Relation::Enemy;
	out.colour[3] = shownCloaked ? config.style.cloak_alpha : 1.0f;
	return true;
}

int32_t HitRect(std::span<const IconRect> rects, float x, float y) {
	for (auto it = rects.rbegin(); it != rects.rend(); ++it) {
		if (std::abs(x - it->x) <= it->w * HALF && std::abs(y - it->y) <= it->h * HALF) return it->unit;
	}
	return -1;
}

bool IsDoubleClick(float seconds, float dx, float dy, bool sameDef, float gap, float px) {
	return sameDef && seconds <= gap && std::abs(dx) <= px && std::abs(dy) <= px;
}

void IconState::Init(int32_t maxIds, int32_t maxInstances) {
	units.Init(maxIds);
	born.Init(units);
	defId.Init(units);
	team.Init(units);
	bits.Init(units);
	kind.Init(units);
	place.Init(units, GONE);
	flickerUntil.Init(units);
	ownColour.Init(units);
	limit = std::max(maxInstances, 0);
	const size_t ids = static_cast<size_t>(units.Capacity());
	order.clear();
	order.reserve(std::max(ids, static_cast<size_t>(limit)));
	instances.clear();
	instances.reserve(static_cast<size_t>(limit));
	contacts.clear();
	contacts.reserve(static_cast<size_t>(limit));
	pending.clear();
	pending.reserve(ids);
	lit.clear();
	lit.reserve(ids);
	dirty.clear();
	dirty.reserve(static_cast<size_t>(limit));
	bornCount = 0;
	changes = 0;
	rebuild = true;
}

void IconCreated(IconState& state, int32_t id) {
	const int32_t slot = Slot(state, id);
	if (slot == DenseIndex::NONE) return;
	state.born[slot] = ++state.bornCount;
	IconMark(state, id);
}

void IconSeen(IconState& state, int32_t id) {
	const int32_t slot = Slot(state, id);
	if (slot == DenseIndex::NONE) return;
	if (state.born[slot] == 0) state.born[slot] = ++state.bornCount;
	IconMark(state, id);
}

void IconGone(IconState& state, int32_t id) {
	const int32_t slot = state.units.SlotOf(id);
	if (slot == DenseIndex::NONE) return;
	const int32_t at = state.place[slot];
	if (at != GONE) {
		state.order[at] = GONE;
		state.instances[at] = IconInstance{};   // width 0: hidden
		SetContact(state, at, false);
		Dirty(state, at);
		++state.changes;
	}
	if (state.flickerUntil[slot] > 0.0f) state.lit.erase(std::find(state.lit.begin(), state.lit.end(), id));
	if ((state.bits[slot] & QUEUED) != 0) state.pending.erase(std::find(state.pending.begin(), state.pending.end(), id));
	const DenseIndex::Move move = state.units.Remove(id);
	state.born.Apply(move);
	state.defId.Apply(move);
	state.team.Apply(move);
	state.bits.Apply(move);
	state.kind.Apply(move);
	state.place.Apply(move);
	state.flickerUntil.Apply(move);
	state.ownColour.Apply(move);
}

void IconMark(IconState& state, int32_t id) {
	const int32_t slot = Slot(state, id);
	if (slot == DenseIndex::NONE || (state.bits[slot] & QUEUED) != 0) return;
	state.bits[slot] = static_cast<uint16_t>(state.bits[slot] | QUEUED);
	state.pending.push_back(id);
}

void IconMarkAll(IconState& state) { state.rebuild = true; }

void RebuildIcons(IconState& state, const IconConfig& config, const IconParams& params,
                  std::span<const IconUnitFacts> facts, std::span<const IconClass> classes, std::span<const Paint> paints) {
	for (int32_t slot = 0; slot < state.units.Size(); ++slot) {
		state.place[slot] = GONE;
		state.kind[slot] = static_cast<uint8_t>(IconKind::None);
		state.bits[slot] = static_cast<uint16_t>(state.bits[slot] & ~QUEUED);
	}
	state.pending.clear();
	state.contacts.clear();
	state.instances.clear();
	state.order.clear();
	const auto classOf = [classes](int32_t def) -> const IconClass* {
		return (def >= 0 && def < static_cast<int32_t>(classes.size())) ? &classes[def] : nullptr;
	};
	const auto paintOf = [paints](int32_t team) -> Paint {
		return (team >= 0 && team < static_cast<int32_t>(paints.size())) ? paints[team] : Paint{};
	};
	// the units that show something; `order` holds indices into `facts` until the instances are written
	IconInstance made;
	for (int32_t i = 0; i < static_cast<int32_t>(facts.size()); ++i) {
		const IconUnitFacts& unit = facts[i];
		const int32_t slot = Slot(state, unit.id);
		if (slot == DenseIndex::NONE) continue;
		const IconClass* cls = classOf(unit.defId);
		const IconKind kind = KindOf(cls, unit, params.pastSwitch);
		Remember(state, slot, unit, BitsOf(config.style, params, unit, kind), kind);
		if (MakeInstance(config, params, cls, unit, paintOf(unit.team), kind, made)) state.order.push_back(i);
		else state.kind[slot] = static_cast<uint8_t>(IconKind::None);
	}
	// creation order, later on top; past the limit the oldest keep their icons
	std::sort(state.order.begin(), state.order.end(), [&state, facts](int32_t a, int32_t b) {
		const uint32_t x = state.born[state.units.SlotOf(facts[a].id)], y = state.born[state.units.SlotOf(facts[b].id)];
		return x != y ? x < y : facts[a].id < facts[b].id;
	});
	if (static_cast<int32_t>(state.order.size()) > state.limit) {
		for (size_t i = state.limit; i < state.order.size(); ++i)
			state.kind[state.units.SlotOf(facts[state.order[i]].id)] = static_cast<uint8_t>(IconKind::None);
		state.order.resize(state.limit);
	}
	for (int32_t at = 0; at < static_cast<int32_t>(state.order.size()); ++at) {
		const IconUnitFacts& unit = facts[state.order[at]];
		const int32_t slot = state.units.SlotOf(unit.id);
		const IconKind kind = static_cast<IconKind>(state.kind[slot]);
		MakeInstance(config, params, classOf(unit.defId), unit, paintOf(unit.team), kind, made);
		state.order[at] = unit.id;
		state.place[slot] = at;
		state.instances.push_back(made);
		Write(state, &config.view, slot, at, made);
		if (kind == IconKind::Contact) state.contacts.push_back(at);
	}
	state.dirty.clear();
	if (!state.instances.empty()) state.dirty.push_back({0, static_cast<int>(state.instances.size())});
	++state.changes;
	state.rebuild = false;
}

bool ApplyIcon(IconState& state, const IconConfig& config, const IconParams& params, const IconUnitFacts& unit,
               const IconClass* cls, const Paint& paint) {
	const int32_t slot = Slot(state, unit.id);
	if (slot == DenseIndex::NONE) return true;
	state.bits[slot] = static_cast<uint16_t>(state.bits[slot] & ~QUEUED);
	IconKind kind = KindOf(cls, unit, params.pastSwitch);
	IconInstance made;
	const bool shown = MakeInstance(config, params, cls, unit, paint, kind, made);
	if (!shown) kind = IconKind::None;
	int32_t at = state.place[slot];
	if (at == GONE && shown) {
		// only the newest unit has its place at the end; any other has to be inserted inside the order
		const bool room = static_cast<int32_t>(state.order.size()) < state.limit;
		if (!room || state.born[slot] <= NewestBorn(state)) {
			state.rebuild = true;
			return false;
		}
		at = static_cast<int32_t>(state.order.size());
		state.order.push_back(unit.id);
		state.instances.push_back(IconInstance{});
		state.place[slot] = at;
	}
	const bool was = state.kind[slot] != static_cast<uint8_t>(IconKind::None);
	Remember(state, slot, unit, BitsOf(config.style, params, unit, kind), kind);
	if (at == GONE || (!shown && !was)) return true;
	Write(state, &config.view, slot, at, shown ? made : IconInstance{});   // a hidden unit keeps its place
	SetContact(state, at, kind == IconKind::Contact);
	Dirty(state, at);
	++state.changes;
	return true;
}

void FlickerStart(IconState& state, const IconViewConfig& view, int32_t id, float seconds) {
	const int32_t slot = state.units.SlotOf(id);
	if (slot == DenseIndex::NONE) return;
	if (state.flickerUntil[slot] <= 0.0f) state.lit.push_back(id);
	state.flickerUntil[slot] = seconds + view.flicker_s;
}

void FlickerStep(IconState& state, const IconViewConfig& view, float seconds) {
	for (size_t i = 0; i < state.lit.size();) {
		const int32_t slot = state.units.SlotOf(state.lit[i]);
		const bool over = seconds >= state.flickerUntil[slot];
		const int32_t at = state.place[slot];
		if (at != GONE && state.kind[slot] == static_cast<uint8_t>(IconKind::Full)) {
			IconInstance& instance = state.instances[at];
			const Rgb own = state.ownColour[slot];
			const float back[3] = {own.r, own.g, own.b};
			const float (&want)[3] = over ? back : view.flicker_colour;
			if (instance.colour[0] != want[0] || instance.colour[1] != want[1] || instance.colour[2] != want[2]) {
				SetColour(instance, want);
				Dirty(state, at);
			}
		}
		if (!over) {
			++i;
			continue;
		}
		state.flickerUntil[slot] = 0.0f;
		state.lit[i] = state.lit.back();
		state.lit.pop_back();
	}
}

} // namespace KT1::Core::IconLexicon
