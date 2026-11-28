/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 core: the minimap's unit layer (see IconMinimapState.h). A port of icon_minimap's Static, PutContact and Rebuild.

#include "IconMinimapState.h"

#include <algorithm>

namespace KT1::Core::IconLexicon {

namespace {

constexpr int AXES = 3;     // DESIGN: red, green, blue
constexpr int CELL_SIDE = 2;   // DESIGN: a cell's third number is its side

const float* CellOf(const IconAtlasEntry& e, int level) {
	switch (level) {
		case 0: return e.l0;
		case 1: return e.l1;
		case 2: return e.l2;   // DESIGN: the atlas's detail levels
		case 3: return e.l3;   // DESIGN
		default: return e.l4;
	}
}

// a radar contact: the grey class shape the sensor's layer gives (air: the fighter's triangle, else the mobile diamond)
void Contact(const IconConfig& config, const IconUnitFacts& unit, MinimapInstance& out) {
	const bool air = LayerOf(unit.pos[1], unit.groundHeight, config.view) == ContactLayer::Air;
	out.cell[0] = static_cast<float>(air ? Shape::Triangle : Shape::Diamond);
	out.cell[3] = 1.0f;   // DESIGN (icon_lex): the shape's scale on the minimap
	for (int i = 0; i < AXES; ++i) out.colour[i] = config.style.contact_grey[i];
	out.mode = static_cast<float>(MM_CONTACT);
	out.size = config.minimap.size_px;
	out.at[3] = 1.0f;
}

bool Silhouette(const IconConfig& config, const IconClass& cls, const Paint& paint, MinimapInstance& out) {
	const bool big = cls.command || cls.tier == TIER_EXPERIMENTAL;
	const float px = big ? config.minimap.big_px : config.minimap.size_px;
	const int level = Lod(px, config.style);
	for (const IconAtlasEntry& e: config.atlas.icons) {
		if (e.key != cls.key) continue;
		const float* cell = CellOf(e, level);
		out.cell[0] = cell[0];
		out.cell[1] = cell[1];
		out.cell[2] = cell[CELL_SIDE];
		out.cell[3] = cell[CELL_SIDE];
		out.colour[0] = paint.colour.r;
		out.colour[1] = paint.colour.g;
		out.colour[2] = paint.colour.b;
		out.mode = static_cast<float>(paint.inverted ? MM_INVERTED : MM_NORMAL);
		out.size = cell[CELL_SIDE] * (px / config.atlas.bake[level]);
		out.turns = cls.rotates ? 1.0f : 0.0f;
		return true;
	}
	return false;
}

} // namespace

void MinimapState::Init(int32_t maxInstances) {
	const size_t n = static_cast<size_t>(std::max(maxInstances, 0));
	order.clear();
	order.reserve(n);
	instances.clear();
	instances.reserve(n);
	contacts.clear();
	contacts.reserve(n);
}

bool MakeMinimapInstance(const IconConfig& config, const IconClass* cls, const IconUnitFacts& unit, const Paint& paint,
                         IconKind kind, MinimapInstance& out) {
	out = MinimapInstance{};
	out.unitSlot = static_cast<float>(unit.unitSlot);
	if (kind == IconKind::Contact) {
		Contact(config, unit, out);
		return true;
	}
	return kind == IconKind::Full && cls != nullptr && Silhouette(config, *cls, paint, out);
}

void MinimapPoint(const float (&pos)[3], const MinimapView& view, float& outX, float& outY) {
	outX = pos[0] / view.mapX * view.w;
	outY = (1.0f - pos[2] / view.mapZ) * view.h;
}

// KT1 hot
void BuildMinimap(MinimapState& state, const IconConfig& config, const IconState& icons, std::span<const IconUnitFacts> facts,
                  std::span<const IconClass> classes, std::span<const Paint> paints, const MinimapView& view) {
	const auto classOf = [classes](int32_t def) -> const IconClass* {
		return (def >= 0 && def < static_cast<int32_t>(classes.size())) ? &classes[def] : nullptr;
	};
	const auto bornOf = [&icons](int32_t id) -> uint32_t {
		const int32_t slot = icons.units.SlotOf(id);
		return slot != DenseIndex::NONE ? icons.born[slot] : 0;
	};
	state.order.clear();
	state.instances.clear();
	state.contacts.clear();
	// `order` holds indices into `facts` until the instances are written
	for (int32_t i = 0; i < static_cast<int32_t>(facts.size()); ++i) {
		if (KindOf(classOf(facts[i].defId), facts[i], true) != IconKind::None) state.order.push_back(i);
	}
	std::sort(state.order.begin(), state.order.end(), [&bornOf, facts](int32_t a, int32_t b) {
		const uint32_t x = bornOf(facts[a].id), y = bornOf(facts[b].id);
		return x != y ? x < y : facts[a].id < facts[b].id;
	});
	size_t kept = 0;
	for (const int32_t i: state.order) {
		if (static_cast<int32_t>(kept) >= icons.limit) break;
		const IconUnitFacts& unit = facts[i];
		const IconClass* cls = classOf(unit.defId);
		const IconKind kind = KindOf(cls, unit, true);
		const bool team = unit.team >= 0 && unit.team < static_cast<int32_t>(paints.size());
		MinimapInstance made;
		if (!MakeMinimapInstance(config, cls, unit, team ? paints[unit.team] : Paint{}, kind, made)) continue;
		if (kind == IconKind::Contact) {
			MinimapPoint(unit.pos, view, made.at[0], made.at[1]);
			state.contacts.push_back(static_cast<int32_t>(kept));
		}
		state.instances.push_back(made);
		state.order[kept++] = unit.id;   // never ahead of the read: kept <= the position of i
	}
	state.order.resize(kept);
}

} // namespace KT1::Core::IconLexicon
