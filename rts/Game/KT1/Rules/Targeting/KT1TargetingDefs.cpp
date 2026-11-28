/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Targeting: the def tables cmb_targeting.lua and unit_cmd_gun.lua build at load (WD, MF, CLASS, classesOf, rank),
// read once at Start from the unit and weapon defs (LUA-FREE S19).

#include "KT1TargetingDefs.h"

#include <cstdlib>
#include <string>

#include "Sim/Units/UnitDef.h"
#include "Sim/Units/UnitDefHandler.h"
#include "Sim/Weapons/WeaponDef.h"
#include "Sim/Weapons/WeaponDefHandler.h"

namespace KT1::Targeting {

namespace TG = Core::Targeting;

namespace {

using Params = spring::unordered_map<std::string, std::string>;

const std::string* Param(const Params& cp, const char* key) {
	const auto it = cp.find(key);
	return (it == cp.end()) ? nullptr : &it->second;
}

bool Is1(const Params& cp, const char* key) { const std::string* v = Param(cp, key); return v != nullptr && *v == "1"; }

// Lua tonumber(cp[key] or def): strtod, then the Lua number (float); text that is no number: nil (here def)
float Num(const Params& cp, const char* key, float def) {
	const std::string* v = Param(cp, key);
	if (v == nullptr) return def;
	char* end = nullptr;
	const double d = std::strtod(v->c_str(), &end);   // KT1 lint: allow double Lua tonumber reads the text with strtod
	return (end == v->c_str()) ? def : static_cast<float>(d);
}

// Lua: for x in s:gmatch("[^" .. sep .. "]+")
std::vector<std::string> Split(const std::string& s, char sep) {
	std::vector<std::string> out;
	std::string cur;
	for (const char ch : s) {
		if (ch == sep) { if (!cur.empty()) out.push_back(cur); cur.clear(); }
		else cur += ch;
	}
	if (!cur.empty()) out.push_back(cur);
	return out;
}

// Lua Reach(cp, targets)
TG::LayerReach Reach(const Params& cp, const std::string& targets) {
	bool air = false, land = false, floor = false, waterSurface = false, under = false;
	for (const std::string& t : Split(targets, ',')) {
		if (t == "air") air = true;
		else if (t == "land") land = true;
		else if (t == "sea floor") floor = true;
		else if (t == "water surface") waterSurface = true;
		else if (t == "under water") under = true;
	}
	TG::LayerReach r;
	r.air = air;
	r.surface = land || floor || waterSurface;
	r.under = under;
	r.seafloor = floor;
	r.above = Is1(cp, "above_water_only");
	r.below = Is1(cp, "below_water_only");
	r.noGround = Is1(cp, "no_ground_target");
	return r;
}

std::string Text(const Params& cp, const char* key, const char* def) {
	const std::string* v = Param(cp, key);
	return (v == nullptr) ? std::string(def) : *v;
}

TG::WeaponTargets ReadWeapon(const WeaponDef* wd, const TG::TargetingConfig& cfg) {
	TG::WeaponTargets w;
	const Params& cp = wd->customParams;
	if (wd->manualfire) {
		const std::string t = Text(cp, "targets", "");
		w.manualFire = true;
		w.manualReach = Reach(cp, t.empty() ? std::string("land,water surface") : t);   // REQ CMD-005 v4 R4, CMB-019 v2 R6
	}
	if (Param(cp, "scan_interval") == nullptr) return w;
	w.scanned = true;
	w.reach = Reach(cp, Text(cp, "targets", ""));
	const std::string pri = Text(cp, "priorities", "any");
	const char sep = (Text(cp, "priorities", "").find('|') != std::string::npos) ? '|' : ',';
	for (const std::string& group : Split(pri, sep)) w.priorities.push_back(Split(group, '+'));
	w.scanInterval = Num(cp, "scan_interval", 0);
	w.track = Num(cp, "tracking_factor", cfg.tracking_factor_default);
	w.retarget = Is1(cp, "recheck_target");
	w.rangeMin = Num(cp, "range_min", 0) * cfg.elmos_per_mu;   // H.mu
	w.direct = Text(cp, "arc", "") != "high" && !Is1(cp, "steer") && !Is1(cp, "underwater");   // CMB-016, CMB-018 R1
	return w;
}

TG::UnitTargets ReadUnit(const UnitDef* ud, const TG::TargetingConfig& cfg) {
	TG::UnitTargets u;
	const Params& cp = ud->customParams;
	u.classes = Split(Text(cp, "target_classes", ""), ',');
	for (size_t n = 0; n < ud->weapons.size() && n < static_cast<size_t>(TG::MAX_UNIT_WEAPONS); ++n)
		u.weapons[n] = (ud->weapons[n].def != nullptr) ? ud->weapons[n].def->id : 0;
	u.armorType = ud->armorType;
	const bool mobile = ud->canmove && ud->speed > 0;   // IsMobile
	u.command = Is1(cp, "command_unit");
	u.mobile = mobile && !u.command;
	u.defence = Is1(cp, "defence");
	u.constructor = mobile && ud->IsBuilderUnit();
	u.plant = ud->IsFactoryUnit() || Is1(cp, "plant");
	u.generator = Num(cp, "eco_production_energy", 0) > 0 || Num(cp, "eco_production_mass", 0) > 0;
	u.sensorPost = Num(cp, "eco_radar", 0) > 0;
	u.barrier = Text(cp, "eco_code", "").find(cfg.barrier_code_part) != std::string::npos;
	u.air = ud->canfly;
	u.armed = !ud->weapons.empty();
	return u;
}

} // namespace

void ReadDefs(TG::TargetingDefs& defs, const TG::TargetingConfig& cfg) {
	const int nw = static_cast<int>(weaponDefHandler->NumWeaponDefs());
	defs.weapons.assign(nw + 1, TG::WeaponTargets{});
	for (int id = 0; id <= nw; ++id) {
		const WeaponDef* wd = weaponDefHandler->GetWeaponDefByID(id);
		if (wd != nullptr) defs.weapons[id] = ReadWeapon(wd, cfg);
	}
	const int nu = unitDefHandler->NumUnitDefs();
	defs.units.assign(nu + 1, TG::UnitTargets{});
	for (int id = 1; id <= nu; ++id) {
		const UnitDef* ud = unitDefHandler->GetUnitDefByID(id);
		if (ud != nullptr) defs.units[id] = ReadUnit(ud, cfg);
	}
	TG::BuildRanks(defs);
	const WeaponDef* gun = weaponDefHandler->GetWeaponDef(cfg.command_gun.weapon);
	defs.gunWeaponDef = (gun != nullptr) ? gun->id : TG::NONE;
	if (gun != nullptr && static_cast<size_t>(gun->id) < defs.weapons.size()) defs.weapons[gun->id].commandGun = true;
	TG::BuildGunRanks(defs, cfg.command_gun.priorities);
}

// Lua: H.mu(tonumber(...)) etc. as the engine step's Init got them
void FeedEngineStep(const TG::TargetingDefs& defs, const TG::TargetingParams& p, const TG::TargetingConfig& cfg) {
	SupComTargeting::Init(1, p.engageInset, p.skipSteps, p.overkillFactor > 0);
	for (size_t id = 0; id < defs.weapons.size(); ++id) {
		const TG::WeaponTargets& w = defs.weapons[id];
		if (!w.scanned) continue;
		SupComTargeting::WeaponRules r;
		r.scan = TG::ScanSteps(p, w.scanInterval);
		r.track = w.track;
		r.rangeMin = w.rangeMin;
		r.retarget = w.retarget;
		r.direct = w.direct;
		r.air = w.reach.air;
		r.surface = w.reach.surface;
		r.under = w.reach.under;
		r.seafloor = w.reach.seafloor;
		r.above = w.reach.above;
		r.below = w.reach.below;
		r.noGround = w.reach.noGround;
		SupComTargeting::SetWeapon(static_cast<int>(id), r);
		for (size_t ud = 0; ud < defs.units.size(); ++ud) SupComTargeting::SetRank(static_cast<int>(id), static_cast<int>(ud), defs.Rank(id, ud));
	}
	(void)cfg;
}

} // namespace KT1::Targeting
