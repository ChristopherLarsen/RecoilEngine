/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 ORDERS: the calls of OrdersInternal.h that work whether or not system Orders is on (LUA-FREE S11): the family's
// config and def facts, built on first use, the queue reads, the orders the family gives and its refusals.

#include "OrdersInternal.h"

#include <algorithm>
#include <array>
#include <memory>
#include <string>
#include <vector>

#include "Game/KT1/Data/DataEngine.h"
#include "Game/KT1/Data/Number.h"
#include "Game/KT1/Host/Check.h"
#include "Lua/LuaRulesParams.h"
#include "Sim/Misc/GlobalConstants.h"
#include "Sim/MoveTypes/MoveDefHandler.h"
#include "Sim/Units/CommandAI/Command.h"
#include "Sim/Units/CommandAI/CommandAI.h"
#include "Sim/Units/CommandAI/FactoryCAI.h"
#include "Sim/Units/Unit.h"
#include "Sim/Units/UnitDef.h"
#include "Sim/Units/UnitDefHandler.h"
#include "Sim/Units/UnitHandler.h"
#include "Sim/Weapons/WeaponDef.h"

namespace KT1::Orders::Internal {

namespace {

using Core::Orders::AcceptWord;
using Core::Orders::CommandKind;
using Core::Orders::DefFacts;
using Core::Orders::DefFlag;
using Core::Orders::OrdersConfig;

constexpr float HALF_FOOTPRINT = SQUARE_SIZE / 2;   // DESIGN: elmos of half footprint per footprint square (the Lua's 4)

struct Shared {
	std::unique_ptr<OrdersConfig> config;
	std::vector<DefFacts> defs;                      // empty: to be built
	std::vector<std::string> handedAccepts;          // by unit def id; empty: the game handed none
};

Shared& Data() {
	static Shared shared;
	return shared;
}

OrdersConfig& MutableConfig() {
	Shared& s = Data();
	if (s.config == nullptr) s.config = std::make_unique<OrdersConfig>(KT1::Data::Load<OrdersConfig>("orders"));
	return *s.config;
}

// ---- unit defs ------------------------------------------------------------------------------------------------------

const std::string* Param(const spring::unordered_map<std::string, std::string>& params, const char* key) {
	const auto it = params.find(key);
	return (it == params.end()) ? nullptr : &it->second;
}

bool Is(const UnitDef& ud, const char* key, const char* value) {
	const std::string* p = Param(ud.customParams, key);
	return p != nullptr && *p == value;
}

// the Lua's tonumber of a custom parameter
bool NumberOf(const spring::unordered_map<std::string, std::string>& params, const char* key, float& out) {
	const std::string* p = Param(params, key);
	return p != nullptr && KT1::Data::ReadNumber(*p, out) == KT1::Data::NumberError::Ok;
}

bool IsNaval(const UnitDef& ud) {
	if (!ud.canmove || ud.canfly || ud.pathType == -1u) return false;
	return moveDefHandler.GetMoveDefByPathType(ud.pathType)->speedModClass == MoveDef::Ship;
}

void SetBodyFlags(const UnitDef& ud, DefFacts& d) {
	const bool command = Is(ud, "command_unit", "1");
	d.Set(DefFlag::CanMove, ud.canmove);
	d.Set(DefFlag::CanFly, ud.canfly);
	d.Set(DefFlag::Building, ud.IsBuildingUnit());
	d.Set(DefFlag::Factory, ud.IsFactoryUnit());
	d.Set(DefFlag::FactoryOrPlant, ud.IsFactoryUnit() || Is(ud, "plant", "1"));
	d.Set(DefFlag::CommandUnit, command);
	d.Set(DefFlag::Reclaimable, !command && !Is(ud, "icon_cmd", "support") && !Is(ud, "eco_reclaimable", "0"));
	d.Set(DefFlag::Submarine, Is(ud, "supcom_submarine", "1"));
	d.Set(DefFlag::StartsSubmerged, Is(ud, "supcom_start_submerged", "1"));
	d.Set(DefFlag::OwnMove, Is(ud, "supcom_move", "1"));
	d.Set(DefFlag::Naval, IsNaval(ud));
}

void SetWorkFlags(const UnitDef& ud, DefFacts& d) {
	const bool works = ud.buildSpeed > 0;
	d.Set(DefFlag::Repairs, ud.canRepair && ud.repairSpeed > 0);
	d.Set(DefFlag::IsBuilder, ud.IsBuilderUnit());
	d.Set(DefFlag::Assists, works && (ud.IsBuilderUnit() || ud.canAssist));
	d.Set(DefFlag::Reclaims, works && ud.canReclaim);
	d.Set(DefFlag::CanReclaim, ud.canReclaim);
}

void SetTransport(const UnitDef& ud, const OrdersConfig& config, DefFacts& d) {
	float land = 0, cargo = 0;
	d.Set(DefFlag::Transport, ud.IsTransportUnit() || ud.transportCapacity > 0);
	d.Set(DefFlag::AirLoadHeight, Param(ud.customParams, "supcom_air_load_height") != nullptr);
	d.Set(DefFlag::UnloadAll, Is(ud, "supcom_unload_all", "1"));
	d.Set(DefFlag::IdleLands,
		Is(ud, "supcom_air", "1") && NumberOf(ud.customParams, "supcom_air_idle_land_delay", land));
	if (ud.transportCapacity > 0 && NumberOf(ud.customParams, "air_land_short_gap", land)
			&& NumberOf(ud.customParams, "air_cargo_short_gap", cargo)) {
		d.landShortGap = land;
		d.cargoShortGap = cargo;
	}
	const std::string* code = Param(ud.customParams, "eco_code");
	for (size_t i = 0; code != nullptr && i < config.transport.codes.size(); ++i) {
		if (config.transport.codes[i].code == *code) d.transportRow = static_cast<int32_t>(i);
	}
}

void SetWeapons(const UnitDef& ud, DefFacts& d) {
	for (unsigned i = 0; i < ud.NumWeapons(); ++i) {
		const WeaponDef* wd = ud.GetWeapon(i).def;
		if (i == 0) d.primaryRange = wd->range;
		if (Param(wd->customParams, "interceptor") == nullptr) d.reach = std::max(d.reach, wd->range);
		const std::string* arc = Param(wd->customParams, "arc");
		if (arc != nullptr && *arc == "high") d.Set(DefFlag::IndirectFire, true);
		float scan = 0;
		if (d.scanPeriodS <= 0 && NumberOf(wd->customParams, "scan_interval", scan) && scan > 0) d.scanPeriodS = scan;
	}
}

// ORD-010 R6: customParams ord_accepts, else what the game handed over, else the config's row for the def's eco code
void SetAccepts(const UnitDef& ud, const OrdersConfig& config, DefFacts& d) {
	const std::string* list = Param(ud.customParams, "ord_accepts");
	const std::vector<std::string>& handed = Data().handedAccepts;
	if (list == nullptr && ud.id < static_cast<int>(handed.size()) && !handed[ud.id].empty()) list = &handed[ud.id];
	const std::string* code = Param(ud.customParams, "eco_code");
	for (size_t i = 0; list == nullptr && code != nullptr && i < config.accepts.size(); ++i) {
		if (config.accepts[i].code == *code) list = &config.accepts[i].words;
	}
	if (list != nullptr) {
		d.Set(DefFlag::HasAccepts, true);
		d.accepts = Core::Orders::ParseAccepts(*list);
	}
	const std::string* spec = Param(ud.customParams, "spc_accepts");
	if (spec != nullptr && !spec->empty()) {
		d.Set(DefFlag::HasSpecAccepts, true);
		d.specAccepts = Core::Orders::ParseAccepts(*spec);
	}
}

DefFacts FactsOf(const UnitDef& ud, const OrdersConfig& config) {
	DefFacts d;
	SetBodyFlags(ud, d);
	SetWorkFlags(ud, d);
	SetTransport(ud, config, d);
	SetWeapons(ud, d);
	SetAccepts(ud, config, d);
	float seen = 0, tier = 0;
	d.vision = NumberOf(ud.customParams, "int_vision", seen) ? seen * config.elmos_per_mu : ud.losRadius;
	if (NumberOf(ud.customParams, "tier", tier)) d.tier = static_cast<int32_t>(tier);
	d.buildReach = ud.buildDistance;
	d.speed = ud.speed;
	d.halfX = ud.xsize * HALF_FOOTPRINT;
	d.halfZ = ud.zsize * HALF_FOOTPRINT;
	for (const Core::Orders::WaypointUnit& w : config.waypoint.units) {
		if (w.unit == ud.name) d.waypointAdvance = w.advance_mu * config.elmos_per_mu;
	}
	return d;
}

// ---- queues ---------------------------------------------------------------------------------------------------------

const CCommandQueue& QueueOf(const CUnit& u) {
	const CFactoryCAI* factory = dynamic_cast<const CFactoryCAI*>(u.commandAI);
	return (factory == nullptr) ? u.commandAI->commandQue : factory->newUnitCommands;
}

} // namespace

const Core::Orders::OrdersConfig& Config() { return MutableConfig(); }

const std::vector<Core::Orders::DefFacts>& Defs() {
	Shared& s = Data();
	if (!s.defs.empty()) return s.defs;
	const OrdersConfig& config = Config();
	const int count = unitDefHandler->NumUnitDefs();
	s.defs.assign(count + 1, DefFacts{});
	for (int id = 1; id <= count; ++id) {
		const UnitDef* ud = unitDefHandler->GetUnitDefByID(id);
		if (ud != nullptr) s.defs[id] = FactsOf(*ud, config);
	}
	return s.defs;
}

void ResetShared() {
	Shared& s = Data();
	s.config.reset();
	s.defs.clear();
	s.handedAccepts.clear();
}

bool HandAccepts(int unitDefId, std::string_view words) {
	if (unitDefId <= 0 || unitDefId > static_cast<int>(unitDefHandler->NumUnitDefs())) return false;
	Shared& s = Data();
	if (unitDefId >= static_cast<int>(s.handedAccepts.size())) s.handedAccepts.resize(unitDefId + 1);
	s.handedAccepts[unitDefId] = std::string(words);
	s.defs.clear();
	return true;
}

Core::Orders::CommandKind KindOf(int commandID) {
	if (commandID < 0) return CommandKind::Build;
	switch (commandID) {
		case CMD_STOP: return CommandKind::Stop;
		case CMD_WAIT: return CommandKind::Wait;
		case CMD_MOVE: return CommandKind::Move;
		case CMD_PATROL: return CommandKind::Patrol;
		case CMD_FIGHT: return CommandKind::Fight;
		case CMD_ATTACK: return CommandKind::Attack;
		case CMD_AREA_ATTACK: return CommandKind::AreaAttack;
		case CMD_MANUALFIRE: return CommandKind::ManualFire;
		case CMD_GUARD: return CommandKind::Guard;
		case CMD_REPAIR: return CommandKind::Repair;
		case CMD_RECLAIM: return CommandKind::Reclaim;
		case CMD_CAPTURE: return CommandKind::Capture;
		case CMD_RESURRECT: return CommandKind::Resurrect;
		case CMD_LOAD_UNITS: return CommandKind::LoadUnits;
		case CMD_LOAD_ONTO: return CommandKind::LoadOnto;
		case CMD_UNLOAD_UNIT: return CommandKind::UnloadUnit;
		case CMD_UNLOAD_UNITS: return CommandKind::UnloadUnits;
		case CMD_FIRE_STATE: return CommandKind::FireState;
		case CMD_MOVE_STATE: return CommandKind::MoveState;
		case CMD_IDLEMODE: return CommandKind::IdleMode;
		case CMD_SELFD: return CommandKind::SelfDestruct;
		case CMD_INSERT: return CommandKind::Insert;
		case CMD_REMOVE: return CommandKind::Remove;
		default: return Core::Orders::CustomKind(commandID);
	}
}

Core::Orders::OrderView ViewOf(const ::Command& command) {
	Core::Orders::OrderView v;
	v.id = command.GetID();
	v.kind = KindOf(v.id);
	v.tag = command.GetTag();
	v.options = command.GetOpts();
	v.internal = command.IsInternalOrder();
	v.shift = (v.options & SHIFT_KEY) != 0;
	const unsigned n = command.GetNumParams();
	v.numParams = static_cast<uint8_t>(std::min<unsigned>(n, UINT8_MAX));
	for (unsigned i = 0; i < n && i < v.params.size(); ++i) v.params[i] = command.GetParam(i);
	return v;
}

void ReadQueue(int unit, int max, std::vector<Core::Orders::OrderView>& out) {
	out.clear();
	const CUnit* u = unitHandler.GetUnit(unit);
	if (u == nullptr) return;
	for (const ::Command& c : QueueOf(*u)) {
		if (max >= 0 && static_cast<int>(out.size()) >= max) return;
		out.push_back(ViewOf(c));
	}
}

bool Give(int unit, int commandID, std::span<const float> params, std::uint8_t options) {
	CUnit* u = unitHandler.GetUnit(unit);
	if (u == nullptr) return false;
	if (Core::Orders::State* state = State(); state != nullptr) {
		Core::Orders::OrderIssue issue;
		issue.unit = unit;
		issue.id = commandID;
		issue.options = options;
		issue.numParams = static_cast<uint8_t>(std::min(params.size(), issue.params.size()));
		std::copy_n(params.begin(), issue.numParams, issue.params.begin());
		state->issued.push_back(issue);
	}
	if (!Check::MayWrite()) return false;
	::Command c(commandID, options);
	for (const float p : params) c.PushParam(p);
	// as Spring.GiveOrderToUnit gives it, so the Allow chains see the same call
	u->commandAI->GiveCommand(c, -1, true, true);   // KT1 lint: allow direct-write a validator has no SimContext; MayWrite is asked above
	return true;
}

void Refuse(int unit, const char* reason, int frame) {
	CUnit* u = unitHandler.GetUnit(unit);
	if (u == nullptr || !Check::MayWrite()) return;
	LuaRulesParams::Param& why = u->modParams["cmb_refused"];   // as Spring.SetUnitRulesParam writes them (private)
	why.value.emplace<std::string>(reason);
	LuaRulesParams::Param& when = u->modParams["cmb_refused_frame"];
	when.value.emplace<float>(static_cast<float>(frame));
}

} // namespace KT1::Orders::Internal
