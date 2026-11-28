/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 draw: keeping the icon set (see IconRuntime.h; LUA-FREE D01, a port of icon_lex's strategic pass): events mark
// units, a changed option, selection, Alt, switch or viewer remakes the set, radar contacts are rewritten each drawn
// frame, and a damaged unit's icon flickers. What a unit then looks like is Core's (Core/IconLexicon/IconState.h).

#include <algorithm>
#include <cmath>
#include <string_view>

#include "IconRuntime.h"

#include "Game/KT1/Draw/Common/FrameFacts.h"
#include "Game/KT1/Draw/FarView/KT1FarView.h"
#include "Game/KT1/Draw/ViewSettings/ZoomGatesApi.h"
#include "Game/KT1/Host/DrawWrites.h"
#include "System/Input/KeyInput.h"

#include <SDL_keycode.h>

namespace KT1::Icons {

namespace {

constexpr float NONE_SIZE = 0.1f;        // DESIGN (icon_lex): the transparent engine icon's size
constexpr float NONE_DISTANCE = 1.0f;    // DESIGN (icon_lex): and its distance factor
constexpr float HALF_UP = 0.5f;          // DESIGN: the switch distance rounds to a whole elmo, as the HUD set it
constexpr int RGBA = 4;                  // DESIGN: red, green, blue, alpha

std::string_view Param(const DrawView& view, int defId, const char* key) {
	std::string_view value;
	return view.UnitDefParam(defId, key, value) ? value : std::string_view();
}

IL::IconClass MakeClass(const DrawView& view, int defId, const IL::IconConfig& config) {
	UnitDefView def;
	if (!view.UnitDefInfo(defId, def)) return {};
	IL::IconDefFacts facts;
	facts.key = Param(view, defId, "icon_key");
	facts.outline = Param(view, defId, "icon_outline");
	facts.glyph = Param(view, defId, "icon_glyph");
	facts.tier = Param(view, defId, "icon_tier");
	facts.command = Param(view, defId, "icon_cmd");
	IL::ParseNumber(Param(view, defId, "icon_size"), facts.size);
	facts.walker = Param(view, defId, "icon_walker") == "1";
	facts.fake = Param(view, defId, "spc_blip") == "1";
	facts.canMove = def.canMove;
	return IL::ClassFor(facts, config);
}

bool Differ(const ViewerView& a, const ViewerView& b) {
	return a.team != b.team || a.allyTeam != b.allyTeam || a.spectating != b.spectating || a.fullView != b.fullView;
}

} // namespace

void Runtime::Init(const IL::IconConfig& with) {
	config = with;
	const int defs = view.UnitDefCount();
	classes.assign(defs + 1, {});
	for (int def = 1; def <= defs; ++def) classes[def] = MakeClass(view, def, config);
	engineIcons.resize(defs + 1);
	const int units = view.UnitCapacity();
	state.Init(units, config.view.max_instances);
	facts.reserve(units);
	scratch.reserve(units);
	rects.reserve(config.view.max_instances);
	paints.reserve(view.TeamCount());
	rectsValid = false;
}

const IL::IconClass* Runtime::Class(int defId) const {
	return (defId > 0 && defId < static_cast<int>(classes.size())) ? &classes[defId] : nullptr;
}

IL::Relation Runtime::RelationOf(const DrawView& from, const ViewerView& who, int team) const {
	if (team == who.team) return IL::Relation::Self;
	return from.TeamAllyTeam(team) == who.allyTeam ? IL::Relation::Ally : IL::Relation::Enemy;
}

IL::Rgb Runtime::TeamFill(const DrawView& from, int team) const {
	float rgba[RGBA] = {HALF_UP, HALF_UP, HALF_UP, 1.0f};   // DESIGN (icon_lex): grey for a team without a colour
	from.TeamColour(team, rgba);
	return IL::FillColour({rgba[0], rgba[1], rgba[2]}, config.style);
}

// KT1 hot
bool Runtime::ReadUnit(const DrawView& from, int id, IL::IconUnitFacts& out) const {
	out = IL::IconUnitFacts{};
	out.id = id;
	UnitStatus s;
	if (!from.Status(id, s)) return false;
	out.defId = s.defId > 0 ? s.defId : -1;
	out.team = s.team;
	out.unitSlot = s.uniformSlot;
	out.relation = RelationOf(from, viewer, s.team);
	out.fullView = viewer.spectating || viewer.fullView;
	out.inLos = s.inLos;
	out.inRadar = s.inRadar;
	out.seenBefore = s.seenBefore;
	out.cloaked = s.cloaked;
	out.health = s.hasHealth ? s.health : 0.0f;
	out.maxHealth = s.hasHealth ? s.maxHealth : 0.0f;
	out.pos[0] = s.pos.x;
	out.pos[1] = s.pos.y;
	out.pos[2] = s.pos.z;
	if (!s.inLos) out.groundHeight = from.GroundHeight(s.pos.x, s.pos.z);
	return true;
}

void Runtime::ReadViewer(const DrawView& from) {
	const ViewerView now = from.Viewer();
	if (!Differ(now, viewer)) return;
	viewer = now;
	IL::IconMarkAll(state);
}

void Runtime::ReadPaints(const DrawView& from) {
	paints.resize(from.TeamCount());
	for (int team = 0; team < static_cast<int>(paints.size()); ++team)
		paints[team] = IL::PaintFor(config, options.relation, false, RelationOf(from, viewer, team), TeamFill(from, team));
}

// the scale option, Alt, the switch and the selection: any change remakes the set
void Runtime::ReadParams(const FrameFacts& frame) {
	const float scale = IL::NearestStep(config.sizes, options.iconScale);
	const bool alt = KeyInput::GetKeyModState(KMOD_ALT);
	const float factor = ZoomGates::DetailFactor(config.view.switch_sweep);
	switchDistance = std::floor(config.view.switch_distance * factor + HALF_UP);
	// the far map view forces icons on at any distance (icon_lex FarViewOn)
	const bool past = frame.ZoomDist() >= switchDistance || FarView::IsFar();
	if (scale != params.scale || alt != params.alt || past != params.pastSwitch) IL::IconMarkAll(state);
	params.scale = scale;
	params.alt = alt;
	params.pastSwitch = past;
	if (frame.SelectionChanges() != selectionChanges) IL::IconMarkAll(state);
	selectionChanges = frame.SelectionChanges();
}

// the topmost box under the pointer: read when the pointer or the camera moved, and every few frames under a still one
void Runtime::Hover(const FrameFacts& frame) {
	const PointerFacts& p = frame.Pointer();
	const CameraFacts& cam = frame.Camera();
	const bool moved = p.x != pointerX || p.y != pointerY || cam.pos != camPos || cam.forward != camForward;
	if (!moved && ++hoverAge < config.view.hover_frames) return;
	pointerX = p.x;
	pointerY = p.y;
	camPos = cam.pos;
	camForward = cam.forward;
	hoverAge = 0;
	const int hit = IL::HitRect(Rects(), static_cast<float>(p.x), static_cast<float>(p.y));
	if (hit == params.hovered) return;
	const int before = params.hovered;
	params.hovered = hit;
	if (before >= 0 && state.units.Has(before)) IL::IconMark(state, before);
	if (hit >= 0) IL::IconMark(state, hit);
}

bool Runtime::ApplyUnit(const DrawContext& ctx, int id) {
	IL::IconUnitFacts unit;
	if (ReadUnit(ctx.view, id, unit)) unit.selected = ctx.facts.IsSelected(id);
	const bool team = unit.team >= 0 && unit.team < static_cast<int>(paints.size());
	return IL::ApplyIcon(state, config, params, unit, Class(unit.defId), team ? paints[unit.team] : IL::Paint{});
}

void Runtime::Apply(const DrawContext& ctx) {
	for (const int32_t id : state.pending) {
		if (!ApplyUnit(ctx, id)) break;   // an insertion inside the order: the rebuild takes the rest
	}
	state.pending.clear();
}

// a radar contact's position carries the radar error, which moves: every contact is rewritten each drawn frame
void Runtime::ApplyContacts(const DrawContext& ctx) {
	scratch.clear();
	for (const int32_t at : state.contacts) scratch.push_back(state.order[at]);
	for (const int32_t id : scratch) {
		if (!ApplyUnit(ctx, id)) return;
	}
}

void Runtime::Rebuild(const DrawContext& ctx) {
	facts.clear();
	IL::IconUnitFacts unit;
	for (const int id : ctx.view.UnitIds()) {
		if (!ReadUnit(ctx.view, id, unit)) continue;
		unit.selected = ctx.facts.IsSelected(id);
		facts.push_back(unit);
	}
	ReadPaints(ctx.view);
	IL::RebuildIcons(state, config, params, facts, classes, paints);
	age = 0;
}

void Runtime::Update(const DrawContext& ctx) {
	ReadViewer(ctx.view);
	ReadParams(ctx.facts);
	Hover(ctx.facts);
	if (++age >= config.view.refresh_frames) IL::IconMarkAll(state);   // a change no event named
	if (!state.rebuild) Apply(ctx);
	if (!state.rebuild) ApplyContacts(ctx);
	if (state.rebuild) Rebuild(ctx);
	IL::FlickerStep(state, config.view, Seconds());
}

// creation order is kept while the system is switched off too, so a set remade later draws in the same order
void Runtime::OnEvent(const DrawFeed::Event& e, bool running) {
	using Kind = DrawFeed::Kind;
	switch (e.kind) {
		case Kind::UnitCreated:
			IL::IconGone(state, e.id);   // an id used again whose old unit left unseen
			IL::IconCreated(state, e.id);
			break;
		case Kind::UnitRemoved: IL::IconGone(state, e.id); break;
		case Kind::UnitEnteredLos:
		case Kind::UnitEnteredRadar: IL::IconSeen(state, e.id); break;
		case Kind::UnitDamaged: {
			if (!running || e.damage <= 0.0f || !state.units.Has(e.id)) break;
			const int32_t slot = state.units.SlotOf(e.id);
			if (state.kind[slot] == static_cast<uint8_t>(IL::IconKind::Full))
				IL::FlickerStart(state, config.view, e.id, Seconds());
			break;
		}
		case Kind::PlayerChanged: IL::IconMarkAll(state); break;
		default:
			if (state.units.Has(e.id)) IL::IconMark(state, e.id);
			break;
	}
}

void Runtime::SwapEngineIcons() {
	if (swapped) return;
	const std::string& none = config.view.engine_icon;
	if (!DrawWrite::IconTypeExists(none))
		DrawWrite::AddIconType(none, config.view.engine_icon_texture, NONE_SIZE, NONE_DISTANCE, false);
	for (int id = 1; id < static_cast<int>(engineIcons.size()); ++id) {
		UnitDefView def;
		if (!view.UnitDefInfo(id, def)) continue;
		if (def.iconType != none) engineIcons[id] = std::string(def.iconType);
		DrawWrite::SetUnitDefIcon(id, none);
	}
	swapped = true;
}

} // namespace KT1::Icons
