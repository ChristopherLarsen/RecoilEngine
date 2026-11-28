/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 draw: the answers of IconSet.h (LUA-FREE D01): the set, its config and classes, the HUD's options, the boxes on
// screen and the HUD's markup. Before the system starts, or in a game without it, each answers "nothing".

#include <string>

#include "IconRuntime.h"

#include "Game/Camera.h"
#include "Game/KT1/Core/IconLexicon/IconHud.h"
#include "Game/KT1/Draw/Common/FrameFacts.h"

namespace KT1::Icons {

namespace {

constexpr float IN_VIEW_RADIUS = 32.0f;   // DESIGN (icon_lex): a unit's point counts as in view with this radius

Runtime* current = nullptr;

bool SameColour(const float (&a)[3], const float (&b)[3]) { return a[0] == b[0] && a[1] == b[1] && a[2] == b[2]; }

bool Same(const IL::HudOptions& a, const IL::HudOptions& b) {
	return a.relation == b.relation && a.iconScale == b.iconScale && a.uiScale == b.uiScale && a.hidden == b.hidden
	       && a.iconsHideWithUi == b.iconsHideWithUi && a.minimapHidden == b.minimapHidden && a.bars == b.bars
	       && a.enemyBars == b.enemyBars && a.paletteChosen == b.paletteChosen && SameColour(a.healthy, b.healthy)
	       && SameColour(a.damaged, b.damaged) && SameColour(a.critical, b.critical);
}

// projects one drawn icon; false: not a unit's icon, hidden, or out of the camera's view
bool BoxOf(const Runtime& r, int32_t place, IL::IconRect& out) {
	const int32_t id = r.state.order[place];
	if (id < 0) return false;
	const int32_t slot = r.state.units.SlotOf(id);
	if (r.state.kind[slot] != static_cast<uint8_t>(IL::IconKind::Full)) return false;
	const IL::IconInstance& icon = r.state.instances[place];
	UnitStatus s;
	if (icon.width <= 0.0f || !r.view.Status(id, s) || !CCamera::GetActive()->InView(s.pos, IN_VIEW_RADIUS)) return false;
	const float3 screen = CCamera::GetActive()->CalcViewPortCoordinates(s.pos);
	out = {id, screen.x, screen.y - icon.lift, icon.width, icon.height};
	return true;
}

} // namespace

Runtime* Current() { return current; }
void SetCurrent(Runtime* runtime) { current = runtime; }

const IL::IconState& Set() {
	static const IL::IconState none;
	return current != nullptr ? current->state : none;
}

const IL::IconConfig& Config() {
	static const IL::IconConfig none;
	return current != nullptr ? current->config : none;
}

const IL::IconClass* ClassOf(int defId) { return current != nullptr ? current->Class(defId) : nullptr; }

std::span<const IL::IconClass> Classes() {
	if (current == nullptr) return {};
	return current->classes;
}

const IL::HudOptions& Options() {
	static const IL::HudOptions none;
	return current != nullptr ? current->options : none;
}

void SetOptions(const IL::HudOptions& options) {
	if (current == nullptr || Same(options, current->options)) return;
	const uint32_t changes = current->options.changes + 1;
	current->options = options;
	current->options.changes = changes;
	IL::IconMarkAll(current->state);
}

float SwitchDistance() { return current != nullptr ? current->switchDistance : -1.0f; }
bool PastSwitch() { return current != nullptr && current->params.pastSwitch; }

// KT1 hot
std::span<const IL::IconRect> Rects() {
	if (current == nullptr) return {};
	Runtime& r = *current;
	const uint32_t frame = DrawPhases::Facts().Frame();
	if (r.rectsValid && frame == r.rectsFrame && r.state.changes == r.rectsChanges) return r.rects;
	r.rects.clear();
	IL::IconRect box;
	for (int32_t place = 0; place < static_cast<int32_t>(r.state.order.size()); ++place) {
		if (BoxOf(r, place, box)) r.rects.push_back(box);
	}
	r.rectsFrame = frame;
	r.rectsChanges = r.state.changes;
	r.rectsValid = true;
	return r.rects;
}

int HitAt(int x, int y) { return IL::HitRect(Rects(), static_cast<float>(x), static_cast<float>(y)); }
int Hovered() { return current != nullptr ? current->params.hovered : -1; }

void SetPaused(bool paused) {
	if (current != nullptr) current->paused = paused;
}

bool Paused() { return current != nullptr && current->paused; }

bool FlickerTest(int unitId) {
	if (current == nullptr || !current->state.units.Has(unitId)) return false;
	const int32_t slot = current->state.units.SlotOf(unitId);
	if (current->state.kind[slot] != static_cast<uint8_t>(IL::IconKind::Full)) return false;
	IL::FlickerStart(current->state, current->config.view, unitId, current->Seconds());
	return true;
}

bool Running() { return current != nullptr && !current->failed; }

bool ReadUnit(const DrawView& view, const ViewerView& who, int id, IL::IconUnitFacts& out) {
	return current != nullptr && current->ReadUnit(view, who, id, out);
}

IL::Paint TeamPaint(const DrawView& view, const ViewerView& who, int team, bool minimap) {
	if (current == nullptr) return {};
	const Runtime& r = *current;
	return IL::PaintFor(r.config, r.options.relation, minimap, r.RelationOf(view, who, team), r.TeamFill(view, team));
}

std::string Markup(int defId, int px, int team, const IL::Badges& badges) {
	if (current == nullptr) return {};
	const Runtime& r = *current;
	const IL::IconClass* cls = r.Class(defId);
	if (cls == nullptr || px <= 0) return {};
	const ViewerView who = r.view.Viewer();
	const int of = team >= 0 ? team : who.team;
	return IL::HudRml(r.config, *cls, px, TeamPaint(r.view, who, of, false), badges);
}

std::string UnitMarkup(int unitId, int px) {
	UnitStatus s;
	if (current == nullptr || !current->view.Status(unitId, s)) return {};
	const Runtime& r = *current;
	const IL::IconClass* cls = r.Class(s.defId);
	if (cls == nullptr) return {};
	IL::IconUnitFacts unit;
	unit.relation = r.RelationOf(r.view, r.view.Viewer(), s.team);
	unit.health = s.hasHealth ? s.health : 0.0f;
	unit.maxHealth = s.hasHealth ? s.maxHealth : 0.0f;
	unit.cloaked = s.cloaked;
	unit.building = s.building;
	unit.idle = s.idle;
	float value = 0.0f;
	if (r.view.UnitRulesParam(unitId, "vet_level", value)) unit.veterancy = static_cast<int>(value);
	std::string_view capacity;
	if (r.view.UnitDefParam(s.defId, "spc_fuel_capacity", capacity)) {
		std::string_view low;
		r.view.UnitDefParam(s.defId, "spc_fuel_low_fraction", low);
		float fuel = 0.0f;
		const bool known = r.view.UnitRulesParam(unitId, "spc_fuel", fuel);
		unit.fuelLow = IL::FuelLow(r.config.style, capacity, low, known, fuel);
	}
	return Markup(s.defId, px, s.team, IL::BadgesFor(r.config.style, *cls, unit));
}

const std::string* EngineIconType(int defId) {
	if (current == nullptr || defId <= 0 || defId >= static_cast<int>(current->engineIcons.size())) return nullptr;
	const std::string& type = current->engineIcons[defId];
	return type.empty() ? nullptr : &type;
}

const char* Kind(int unitId) {
	if (current == nullptr || !current->state.units.Has(unitId)) return nullptr;
	switch (static_cast<IL::IconKind>(current->state.kind[current->state.units.SlotOf(unitId)])) {
		case IL::IconKind::Full: return "full";
		case IL::IconKind::Contact: return "contact";
		default: return nullptr;
	}
}

int KindCount() {
	if (current == nullptr) return 0;
	const IL::IconState& state = current->state;
	int count = 0;
	for (int32_t slot = 0; slot < state.units.Size(); ++slot)
		count += state.kind[slot] != static_cast<uint8_t>(IL::IconKind::None);
	return count;
}

const char* Failure() {
	if (current == nullptr) return "the system has not started";
	return current->failed ? current->why.c_str() : nullptr;
}

} // namespace KT1::Icons
