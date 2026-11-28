/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 rule: HullTurn (version 1). A unit whose target is outside every weapon's arc turns its hull to bring it in
// (REQ MOV-005 v3 R6, v4 R7). Ported from cmb_hullturn.lua as the Lua has it (LUA-FREE S19; port, then correct).
//
// Replaces:   LuaRules/Gadgets/cmb_hullturn.lua (it runs when this rule is off or in check mode)
// Switch:     mod option kt1_hullturn = 0 (default) / 1 / check
// Config:     gamedata/kt1/hullturn.lua (HullTurnConfig.h has the keys, defaults and labels)
// Phases:     FrameStart  tracks finished and destroyed units; visits the due ones (turning, or (f + id) % check == 0)
//             (the Lua's GameFrame at layer 10: no gadget before it writes headings, so FrameStart sees what it saw)
// State:      per unit: turning, free (Core/HullTurn); in the sync checksum when on
// Writes:     CSolidObject::ForcedSpin (as Spring.SetUnitDirection); CSupComGroundMoveType::SetHeadingFree (as
//             Spring.MoveCtrl.SetGroundMoveTypeData "supcomHeadingFree"); both through c.Write, skipped in check
// Check:      Check::Native "face" (the heading, radians) and "turning" per visited unit; the gadget answers the same
//             visit with Spring.KT1.Check.Answer("HullTurn", "face" | "turning", unitID, 0, 0, value)
// Lua:        Spring.KT1.HullTurn.Turning(unitID), .Tracked(unitID): GG.HullTurn's two readers
// Determinism: visits in ascending unit id (the Lua's list order cannot reach a result: a visit writes only its own
//             unit's heading and reads no other unit's); maths in Core in the Lua's order, trig from the engine.
// Doc:        game-build docs/engine/rules/hullturn.md

#include "HullTurnConfig.h"

#include <algorithm>
#include <cstdlib>
#include <vector>

#include "Game/KT1/Core/HullTurn/HullTurnCore.h"
#include "Game/KT1/Core/Common/Rate.h"
#include "Game/KT1/Host/Check.h"
#include "Game/KT1/Host/Phases.h"
#include "Game/KT1/Host/SimContext.h"
#include "Game/KT1/Host/SyncState.h"
#include "Game/KT1/KT1Register.h"
#include "Game/KT1/KT1Registry.h"
#include "Lua/LuaHandle.h"
#include "Lua/LuaParser.h"
#include "Lua/LuaUtils.h"
#include "Sim/Misc/GlobalConstants.h"
#include "Sim/MoveTypes/GroundMoveType.h"
#include "Sim/MoveTypes/MoveDefHandler.h"
#include "Sim/MoveTypes/SupComGroundMoveType.h"
#include "Sim/Units/CommandAI/Command.h"
#include "Sim/Units/CommandAI/CommandAI.h"
#include "Sim/Units/Unit.h"
#include "Sim/Units/UnitDef.h"
#include "Sim/Units/UnitDefHandler.h"
#include "Sim/Units/UnitHandler.h"
#include "Sim/Weapons/Weapon.h"
#include "Sim/Weapons/WeaponDef.h"
#include "System/SpringMath.h"

namespace KT1 {
namespace {

namespace HT = Core::HullTurn;
using Core::SimHost::Event;
using Core::SimHost::EventType;
using Core::SimHost::MaskOf;
using Core::SimHost::Phase;

constexpr const char* NAME = "HullTurn";
constexpr int PHASE_ORDER = 100;            // DESIGN: after Targeting (0) in FrameStart: the Lua gadget had layer 10, the gun gadget 0
constexpr float FULL_ARC_DEG = 180;          // DESIGN: a weapon with no yaw_range turns all round (the Lua default)
constexpr float FIRE_TOLERANCE_DEG = 2;      // DESIGN: the Lua default fire_tolerance
constexpr float ANGLE_UNITS = 65536;         // DESIGN (engine fact): the turn rate counts a full turn as 65536 angle units
constexpr int32_t GROUND_PARAMS = 3;         // DESIGN (engine fact): a command with x, y, z is a ground point
constexpr int32_t KIND_POS = 2;              // DESIGN (engine fact): the Lua weapon-target kind of a ground point (Target_Pos)

float EngAtan2(float y, float x) { return math::atan2(y, x); }
float EngSin(float a) { return math::sin(a); }
float EngCos(float a) { return math::cos(a); }

// Lua: tonumber(cp[key] or def): a string becomes a number by strtod, then the Lua number (float)
float ParamNum(const spring::unordered_map<std::string, std::string>& cp, const char* key, float def) {
	const auto it = cp.find(key);
	if (it == cp.end()) return def;
	return static_cast<float>(std::strtod(it->second.c_str(), nullptr));
}

class HullTurn : public Rule {
public:
	HullTurn() : Rule({NAME, 1, true, "LuaRules/Gadgets/cmb_hullturn.lua", false, true}) {}

	void LoadConfig(const LuaTable& cfg) override {
		config.check_s = cfg.GetFloat("check_s", config.check_s);
		config.rest_speed_s = cfg.GetFloat("rest_speed_s", config.rest_speed_s);
		config.done_rad = cfg.GetFloat("done_rad", config.done_rad);
		config.axis_rad = cfg.GetFloat("axis_rad", config.axis_rad);
		config.attack_move_cmd = cfg.GetInt("attack_move_cmd", config.attack_move_cmd);
		checkSteps = Core::Rate::Every(config.check_s);
		// per step in float: the bits of the Lua's per-frame literal at 30 steps per second
		params.rest = config.rest_speed_s / static_cast<float>(Core::Rate::StepsPerSecond());
		params.done = config.done_rad;
		params.axis = config.axis_rad;
		trig = {EngAtan2, EngSin, EngCos, math::PI};
		units.assign(MAX_UNITS, Unit{});
		RegisterPhases();
		if (SwitchMode() != Mode::Check) RegisterSync();
	}

	void ConfigValues(Data::Values& v) const override {
		v.Add("check_s", config.check_s);
		v.Add("rest_speed_s", config.rest_speed_s);
		v.Add("done_rad", config.done_rad);
		v.Add("axis_rad", config.axis_rad);
	}

	void Start() override { ReadDefs(); }

	void PushLua(lua_State* L) override {
		LuaPushRawNamedCFunc(L, "Turning", LTurning);
		LuaPushRawNamedCFunc(L, "Tracked", LTracked);
	}

private:
	struct Unit {
		bool tracked = false;
		int32_t def = 0;
		HT::UnitState s;
	};

	// Lua: the spec table, one entry per def that can turn and has a weapon with a limited arc
	void ReadDefs() {
		const int n = unitDefHandler->NumUnitDefs();
		specs.assign(n + 1, HT::DefSpec{});
		has.assign(n + 1, 0);
		const float pi = math::PI;
		const float tau = pi + pi;   // Lua: 2 * math.pi
		for (int d = 1; d <= n; ++d) {
			const UnitDef* ud = unitDefHandler->GetUnitDefByID(d);
			if (ud == nullptr || !ud->canmove || ud->canfly || !(ud->turnRate > 0) || ud->weapons.empty()) continue;
			HT::DefSpec s;
			bool limited = false;
			for (const UnitDefWeapon& w : ud->weapons) {
				if (s.weapons >= HT::MAX_WEAPONS || w.def == nullptr) break;
				const float arc = ParamNum(w.def->customParams, "yaw_range", FULL_ARC_DEG) * math::DEG_TO_RAD;
				const float tol = ParamNum(w.def->customParams, "fire_tolerance", FIRE_TOLERANCE_DEG) * math::DEG_TO_RAD;
				s.w[s.weapons++] = {arc, tol};
				if (arc < pi) limited = true;
			}
			const MoveDef* md = (ud->pathType != -1U) ? moveDefHandler.GetMoveDefByPathType(ud->pathType) : nullptr;
			s.hover = (md != nullptr && md->name.find("hover") != std::string::npos)
				|| ud->customParams.find("supcom_hover_height") != ud->customParams.end();
			s.step = ud->turnRate * tau / ANGLE_UNITS;
			if (limited) { specs[d] = s; has[d] = 1; }
		}
	}

	void RegisterPhases() {
		const auto mask = MaskOf(EventType::UnitFinished) | MaskOf(EventType::UnitDestroyed);
		Phases::Add(NAME, Phase::FrameStart, PHASE_ORDER, mask, [this](const SimContext& c) { Track(c); Visit(c); });
	}

	void RegisterSync() {
		SyncState::Add(NAME, "units", [this](Core::SimHost::StateHash& h) {
			for (const int32_t id : ids) {
				const Unit& u = units[id];
				h.Add(id);
				h.Add(static_cast<int32_t>(u.s.turning) | (static_cast<int32_t>(u.s.free) << 1));
			}
			h.Add(static_cast<int32_t>(shared.switchOK));
		});
	}

	void Track(const SimContext& c) {
		c.events.ForEachRaised([this](const Event& e) {
			if (e.id < 0 || e.id >= static_cast<int32_t>(units.size())) return;
			if (e.type == EventType::UnitFinished) Add(e.id, e.def);
			else Drop(e.id);
		});
	}

	void Add(int32_t id, int32_t def) {
		if (def < 0 || def >= static_cast<int32_t>(has.size()) || !has[def] || units[id].tracked) return;
		units[id] = Unit{true, def, {}};
		ids.insert(std::lower_bound(ids.begin(), ids.end(), id), id);
	}

	void Drop(int32_t id) {
		if (!units[id].tracked) return;
		units[id] = Unit{};
		ids.erase(std::lower_bound(ids.begin(), ids.end(), id));
	}

	// KT1 hot
	void Visit(const SimContext& c) {
		for (const int32_t id : ids) {
			if (!c.ids.Contains(id)) continue;
			Unit& r = units[id];
			if (!HT::Due(r.s, c.frame, id, checkSteps)) continue;
			CUnit* u = unitHandler.GetUnit(id);
			if (u == nullptr) continue;
			const HT::DefSpec& s = specs[r.def];
			HT::View v;
			Read(u, s, v);
			const HT::Out o = HT::Update(r.s, s, v, params, trig, shared);
			Apply(c, u, o);
			if (c.check) {
				if (o.face) Check::Native(NAME, "face", id, 0, 0, o.faceA);
				Check::Native(NAME, "turning", id, 0, 0, r.s.turning ? 1.0f : 0.0f);
			}
		}
	}

	// KT1 hot
	void Read(const CUnit* u, const HT::DefSpec& s, HT::View& v) const {
		const CCommandQueue& q = u->commandAI->commandQue;
		if (!q.empty()) {
			const Command& cmd = q.front();
			const int id = cmd.GetID();
			v.nparams = static_cast<int32_t>(cmd.GetNumParams());
			if (v.nparams >= 1) v.p1 = cmd.GetParam(0);
			if (v.nparams >= GROUND_PARAMS) v.p3 = cmd.GetParam(GROUND_PARAMS - 1);
			if (id == CMD_ATTACK) v.cmd = HT::Cmd::Attack;
			else if (id == CMD_MOVE || id == CMD_PATROL || id == CMD_FIGHT || id == config.attack_move_cmd) v.cmd = HT::Cmd::Moving;
			else v.cmd = HT::Cmd::Other;
			if (v.cmd == HT::Cmd::Attack && v.nparams >= 1 && v.nparams < GROUND_PARAMS) {
				const CUnit* t = unitHandler.GetUnit(static_cast<int>(v.p1));
				if (t != nullptr) { v.p1Unit = true; v.p1x = t->pos.x; v.p1z = t->pos.z; }
			}
		}
		v.fireState = u->fireState;
		v.vx = u->speed.x; v.vz = u->speed.z; v.speed = u->speed.w;
		v.ux = u->pos.x; v.uz = u->pos.z;
		v.dx = u->frontdir.x; v.dz = u->frontdir.z;
		v.weapons = static_cast<int32_t>(std::min<size_t>(u->weapons.size(), HT::MAX_WEAPONS));
		for (int32_t n = 0; n < v.weapons; ++n) v.range[n] = u->weapons[n]->range;
		for (int32_t n = 0; n < s.weapons && n < v.weapons && v.wkind == 0; ++n) {
			const SWeaponTarget& t = u->weapons[n]->GetCurrentTarget();
			if (t.type == Target_Unit && t.unit != nullptr) { v.wkind = 1; v.wx = t.unit->pos.x; v.wz = t.unit->pos.z; }
			else if (t.type == Target_Pos) { v.wkind = KIND_POS; v.wx = t.groundPos.x; v.wz = t.groundPos.z; }
		}
		v.groundMoveType = dynamic_cast<const CGroundMoveType*>(u->moveType) != nullptr;
	}

	void Apply(const SimContext& c, CUnit* u, const HT::Out& o) const {
		for (int32_t i = 0; i < o.freeWrites; ++i) {
			const bool on = o.free[i];
			c.Write([u, on]() {
				if (auto* mt = dynamic_cast<CSupComGroundMoveType*>(u->moveType)) mt->SetHeadingFree(on);
			});
		}
		if (o.face) {
			const float3 dir = float3(o.fx, 0, o.fz).SafeNormalize();
			c.Write([u, dir]() { u->ForcedSpin(dir); });
		}
	}

	static HullTurn* Self(lua_State* L) {
		HullTurn* r = static_cast<HullTurn*>(Registry::Get().Find(NAME));
		return (r != nullptr && r->Enabled() && CLuaHandle::GetHandleSynced(L)) ? r : nullptr;
	}
	static const Unit* Tracked(lua_State* L) {
		const HullTurn* r = Self(L);
		const int id = luaL_checkint(L, 1);
		if (r == nullptr || id < 0 || id >= static_cast<int>(r->units.size()) || !r->units[id].tracked) return nullptr;
		return &r->units[id];
	}
	static int LTurning(lua_State* L) {
		const Unit* u = Tracked(L);
		lua_pushboolean(L, u != nullptr && u->s.turning);
		return 1;
	}
	static int LTracked(lua_State* L) {
		lua_pushboolean(L, Tracked(L) != nullptr);
		return 1;
	}

	HullTurnConfig config;
	int32_t checkSteps = 1;
	HT::Params params;
	HT::Trig trig{};
	HT::Shared shared;
	std::vector<HT::DefSpec> specs;
	std::vector<uint8_t> has;
	std::vector<Unit> units;      // by unit id
	std::vector<int32_t> ids;     // tracked ids, ascending
};

} // namespace

// ORDER: 2100, phase "before move", after Targeting (2000)
KT1_REGISTER_SYSTEM(HullTurn, 2100, HullTurn)

} // namespace KT1
