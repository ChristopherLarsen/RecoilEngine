/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Cargo: CargoApi.h over Core/Cargo/CargoRoom (LUA-FREE S12).

#include "CargoApi.h"

#include <cmath>
#include <cstdlib>
#include <string>

#include "Game/KT1/Core/Cargo/CargoRoom.h"
#include "Game/KT1/Core/Common/Rate.h"
#include "Game/KT1/Host/Check.h"
#include "Game/KT1/Data/DataEngine.h"
#include "Lua/LuaParser.h"
#include "Lua/LuaRulesParams.h"
#include "Sim/Units/Unit.h"
#include "Sim/Units/UnitDef.h"
#include "Sim/Units/UnitDefHandler.h"
#include "Sim/Units/UnitHandler.h"
#include "System/FileSystem/VFSModes.h"

namespace KT1::Cargo {

namespace CC = Core::Cargo;

namespace {

constexpr const char* CONFIG = "gamedata/kt1/cargo.lua";
constexpr int MAX_CLASS_KEY = 32;          // DESIGN: carry_class keys looked at in size_of_class (the pack uses 1-10)
constexpr float NOTE_JOIN_S = 2;           // PLACEHOLDER (spc_cargo.lua NOTE_JOIN = 2 * FPS)

struct Def {
	bool hasSlots = false;
	CC::Room slots{};
	int32_t capacity = 1;
	int32_t size = CC::NO_SIZE;
};

CC::RoomModel model;
std::vector<Def> defs;                     // by unit def id

// Lua tonumber(x) or 0
float Num(const spring::unordered_map<std::string, std::string>& cp, const char* key) {
	const auto it = cp.find(key);
	if (it == cp.end()) return 0;
	char* end = nullptr;
	const double d = std::strtod(it->second.c_str(), &end);   // KT1 lint: allow double Lua tonumber reads the text with strtod
	return (end == it->second.c_str()) ? 0 : static_cast<float>(d);
}

void ReadRows(const LuaTable& t, std::array<CC::Room, CC::CLASSES>& rows) {
	for (int z = 0; z < CC::CLASSES; ++z) {
		const LuaTable row = t.SubTable(z + 1);
		for (int k = 0; k < CC::CLASSES; ++k) rows[z][k] = row.GetFloat(k + 1, 0);
	}
}

CC::Transport ViewOf(const CUnit* t) {
	CC::Transport v;
	const Def& d = defs[t->unitDef->id];
	v.hasSlots = d.hasSlots;
	v.slots = d.slots;
	v.capacity = d.capacity;
	for (const CUnit::TransportedUnit& tu : t->transportedUnits)
		v.aboard.push_back(tu.unit != nullptr ? defs[tu.unit->unitDef->id].size : CC::NO_SIZE);
	return v;
}

bool Known(const CUnit* u) { return u != nullptr && u->unitDef->id >= 0 && static_cast<size_t>(u->unitDef->id) < defs.size(); }

} // namespace

void Init() {
	LuaParser parser(CONFIG, SPRING_VFS_MOD_BASE, SPRING_VFS_ZIP, {true}, {true});
	KT1::Data::AddModOptions(parser);
	parser.Execute();
	const LuaTable root = parser.GetRoot();
	model = CC::RoomModel{};
	ReadRows(root.SubTable("need"), model.need);
	ReadRows(root.SubTable("use"), model.use);
	const LuaTable size = root.SubTable("size_of_class");
	const int n = unitDefHandler->NumUnitDefs();
	defs.assign(n + 1, Def{});
	for (int id = 1; id <= n; ++id) {
		const UnitDef* ud = unitDefHandler->GetUnitDefByID(id);
		if (ud == nullptr) continue;
		const auto& cp = ud->customParams;
		Def& d = defs[id];
		d.hasSlots = cp.count("spc_slots_small") || cp.count("spc_slots_medium") || cp.count("spc_slots_large");
		d.slots = {Num(cp, "spc_slots_small"), Num(cp, "spc_slots_medium"), Num(cp, "spc_slots_large")};
		d.capacity = ud->transportCapacity;   // the Lua's `or 1` only applies to a missing field; the engine always has one
		const int cls = static_cast<int>(Num(cp, "carry_class"));
		if (cls >= 1 && cls <= MAX_CLASS_KEY) {
			const int z = size.GetInt(cls, 0);
			d.size = (z >= 1 && z <= CC::CLASSES) ? z - 1 : CC::NO_SIZE;
		}
	}
}

void SelectLoad(int32_t transport, std::span<const int32_t> cands, std::vector<int32_t>& taken, std::vector<int32_t>& left) {
	taken.clear();
	left.clear();
	const CUnit* t = unitHandler.GetUnit(transport);
	if (!Known(t)) return;
	std::vector<int32_t> sizes;
	for (const int32_t c : cands) {
		const CUnit* u = unitHandler.GetUnit(c);
		sizes.push_back(Known(u) ? defs[u->unitDef->id].size : CC::NO_SIZE);
	}
	std::vector<int32_t> ti, li;
	CC::SelectLoad(ViewOf(t), sizes, model, ti, li);
	for (const int32_t i : ti) taken.push_back(cands[i]);
	for (const int32_t i : li) left.push_back(cands[i]);
}

bool Full(int32_t transport) {
	const CUnit* t = unitHandler.GetUnit(transport);
	return Known(t) && CC::Full(ViewOf(t), model);
}

// Lua GG.SpcCargo.NoteRefused(t, n, add)
void NoteRefused(int32_t transport, int32_t n, bool add, int32_t frame) {
	CUnit* t = unitHandler.GetUnit(transport);
	if (n <= 0 || t == nullptr || !Check::MayWrite()) return;
	if (add) {
		// the last note, from the same rules params spc_cargo.lua writes
		const auto pn = t->modParams.find("cargo_refused_n");
		const auto pf = t->modParams.find("cargo_refused_f");
		if (pn != t->modParams.end() && pf != t->modParams.end()) {
			const float* nv = std::get_if<float>(&pn->second.value);
			const float* fv = std::get_if<float>(&pf->second.value);
			const float join = NOTE_JOIN_S * static_cast<float>(Core::Rate::StepsPerSecond());
			if (nv != nullptr && fv != nullptr && static_cast<float>(frame) - *fv <= join) n = n + static_cast<int32_t>(*nv);
		}
	}
	// Lua: Spring.SetUnitRulesParam(t, key, value, { allied = true })
	auto& rn = t->modParams["cargo_refused_n"];
	rn.value.emplace<float>(static_cast<float>(n));
	rn.los = LuaRulesParams::RULESPARAMLOS_PRIVATE | LuaRulesParams::RULESPARAMLOS_ALLIED;
	auto& rf = t->modParams["cargo_refused_f"];
	rf.value.emplace<float>(static_cast<float>(frame));
	rf.los = LuaRulesParams::RULESPARAMLOS_PRIVATE | LuaRulesParams::RULESPARAMLOS_ALLIED;
}

} // namespace KT1::Cargo
