/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Targeting core (LUA-FREE S19): cmb_targeting.lua and unit_cmd_gun.lua around the fork's per-step choice, as the Lua
// has them (port, then correct). No engine include. Every number is the Lua number (float), in the Lua's order.

#include "Game/KT1/Core/Targeting/TargetState.h"

#include "Game/KT1/Core/Common/DetMath.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace KT1::Core::Targeting {

namespace {

constexpr int32_t LOS_BITS = 8;          // DESIGN (engine fact): 1 sight, 2 radar, 4 seen before; the Lua reads b % 8
constexpr float HALF = DetMath::HALF;
constexpr float FAR = std::numeric_limits<float>::max();   // the Lua math.huge of a distance with a unit gone
constexpr float SCAN_EPS = 1e-6f;          // DESIGN (the Lua ScanFrames): keeps 0.3 / 0.1 = 2.9999999 at 3 ticks
constexpr int32_t INSERT_WHAT = 1;         // DESIGN (engine fact): CMD.INSERT parameters: position, command, options
constexpr int32_t INSERT_OWN = 3;          // DESIGN (engine fact): where an inserted order's own parameters start
constexpr int32_t INSERT_OWN_MAX = 3;      // DESIGN: the Lua copies parameters 4 to 6

// Lua: opt % (2 * OPT_INTERNAL) >= OPT_INTERNAL, the internal bit of the options (one bit, options never negative)
bool Internal(int32_t options, int32_t bit) { return (options & bit) != 0; }


// Lua: math.sqrt((ax - bx) ^ 2 + (az - bz) ^ 2); x ^ 2 is x * x for a float
float HDist(const float (&a)[3], const float (&b)[3]) {
	const float dx = a[0] - b[0], dz = a[2] - b[2];
	return std::sqrt(dx * dx + dz * dz);
}

bool HDistUnits(TargetingWorld& world, int32_t a, int32_t b, float& d) {
	float pa[3], pb[3];
	if (!world.Position(a, pa) || !world.Position(b, pb)) return false;
	d = HDist(pa, pb);
	return true;
}

// Lua LineClear (REQ CMB-016 v2 R2): the straight path from gun to target, sampled every line_sample_step elmos
bool LineClear(const TargetingParams& p, TargetingWorld& world, int32_t u, int32_t t) {
	float a[3], b[3];
	if (!world.MidPosition(u, a) || !world.MidPosition(t, b)) return true;
	const float dx = b[0] - a[0], dz = b[2] - a[2];
	const int32_t n = static_cast<int32_t>(std::floor(std::sqrt(dx * dx + dz * dz) / p.lineSampleStep));
	for (int32_t i = 1; i <= n - 1; ++i) {
		const float k = static_cast<float>(i) / static_cast<float>(n);
		if (world.GroundHeight(a[0] + dx * k, a[2] + dz * k) > a[1] + (b[1] - a[1]) * k + p.lineClearance) return false;
	}
	return true;
}

// Lua FriendsClear (REQ CMB-003 R6: off unless a config turns it on)
bool FriendsClear(const TargetingContext& c, TargetingWorld& world, int32_t u, int32_t t) {
	const TargetingParams& p = c.params;
	if (!p.lineFriends) return true;
	float a[3], b[3];
	if (!world.Position(u, a) || !world.Position(t, b)) return true;
	const float dx = b[0] - a[0], dz = b[2] - a[2];
	const float l2 = dx * dx + dz * dz;
	if (l2 <= 0) return true;
	const float pad = p.lineFriendPad;
	const int32_t ally = world.AllyTeam(u);
	std::vector<int32_t> near;
	world.UnitsInRectangle(std::min(a[0], b[0]) - pad, std::min(a[2], b[2]) - pad, std::max(a[0], b[0]) + pad,
		std::max(a[2], b[2]) + pad, near);
	for (const int32_t o : near) {
		if (o == u || o == t || world.AllyTeam(o) != ally) continue;
		const int32_t od = world.UnitDef(o);
		if (od >= 0 && static_cast<size_t>(od) < c.defs.units.size() && c.defs.units[od].air) continue;
		float q[3];
		if (!world.Position(o, q)) continue;
		const float k = ((q[0] - a[0]) * dx + (q[2] - a[2]) * dz) / l2;
		if (k > 0 && k < 1) {
			const float px = a[0] + dx * k - q[0], pz = a[2] + dz * k - q[2];
			const float r = world.Radius(o);
			if (px * px + pz * pz < r * r) return false;
		}
	}
	return true;
}

const WeaponTargets* WeaponOf(const TargetingDefs& d, int32_t weaponDef) {
	return (weaponDef >= 0 && static_cast<size_t>(weaponDef) < d.weapons.size()) ? &d.weapons[weaponDef] : nullptr;
}

const UnitTargets* UnitOf(const TargetingDefs& d, int32_t unitDef) {
	return (unitDef >= 0 && static_cast<size_t>(unitDef) < d.units.size()) ? &d.units[unitDef] : nullptr;
}

// Lua LayerOK for a target unit
Refusal LayerOf(const LayerReach& r, int32_t t, const TargetingContext& c, TargetingWorld& world) {
	const UnitTargets* td = UnitOf(c.defs, world.UnitDef(t));
	float pos[3] = {0, 0, 0};
	const bool hasPos = world.Position(t, pos);
	const Layer layer = world.MovementLayer(t);
	// no position: the Lua's `y and ...` is false, the land rule stands in
	return LayerCheck(r, td != nullptr && td->air, layer, hasPos ? pos[1] : 0, hasPos ? world.Height(t) : 0);
}

bool Overkill(const TargetingParams& p) { return p.overkillFactor > 0; }

} // namespace

const char* RefusalName(Refusal reason) {
	switch (reason) {
		case Refusal::BelowWaterOnly: return "below_water_only";
		case Refusal::NoAirTarget: return "no_air_target";
		case Refusal::AirToAirOnly: return "air_to_air_only";
		case Refusal::TargetBelowWater: return "target_below_water";
		case Refusal::NoSurfaceTarget: return "no_surface_target";
		case Refusal::TargetAboveWater: return "target_above_water";
		case Refusal::CannotTarget: return "cannot_target";
		case Refusal::None: break;
	}
	return "";
}

TargetingParams MakeTargetingParams(const TargetingConfig& config, const CommandIds& commands, int32_t stepsPerSecond) {
	TargetingParams p;
	p.stepsPerSecond = stepsPerSecond;
	const float fps = static_cast<float>(stepsPerSecond);
	// Lua H.FRAMES_PER_TICK = math.max(1, math.floor(H.REF_TICK * H.FPS + 0.5))
	p.stepsPerTick = std::max<int32_t>(1, static_cast<int32_t>(std::floor(config.ref_tick_s * fps + HALF)));
	p.refTick = config.ref_tick_s;
	p.extraTicks = config.scan_extra_ticks;
	// Lua H.s_to_frames(s) = math.floor(s * H.FPS + 0.5)
	p.skipSteps = static_cast<int32_t>(std::floor(config.retarget_skip_s * fps + HALF));
	p.engageInset = config.engage_inset_mu * config.elmos_per_mu;   // H.mu
	p.overkillFactor = config.overkill_inflight_factor;
	p.lineSampleStep = config.line_sample_step;
	p.lineClearance = config.line_clearance;
	p.lineFriends = config.line_friends;
	p.lineFriendPad = config.line_friend_pad;
	p.inflightEpsilon = config.inflight_epsilon;
	p.scanPhaseFactor = config.scan_phase_factor;
	p.gunScanSteps = ScanSteps(p, config.command_gun.scan_interval_s);
	p.gunInset = config.command_gun.engage_inset_mu * config.elmos_per_mu;
	p.gunTrack = config.command_gun.tracking_factor;
	p.gunRangeMin = config.command_gun.range_min_mu * config.elmos_per_mu;
	p.gunRangeMax = config.command_gun.range_max_mu * config.elmos_per_mu;
	p.commands = commands;
	return p;
}

// Lua ScanFrames: (math.max(1, math.ceil(s / H.REF_TICK - 1e-6)) + scan_extra_tick) * H.FRAMES_PER_TICK
int32_t ScanSteps(const TargetingParams& p, float intervalSeconds) {
	const float eps = SCAN_EPS;
	const int32_t ticks = std::max<int32_t>(1, static_cast<int32_t>(std::ceil(intervalSeconds / p.refTick - eps))) + p.extraTicks;
	return ticks * p.stepsPerTick;
}

// Lua UnitCreated: f + 1 + (u * 7 + n) % every
int32_t FirstScan(const TargetingParams& p, int32_t unit, int32_t weapon, int32_t scanSteps, int32_t step) {
	return step + 1 + (unit * p.scanPhaseFactor + weapon) % scanSteps;
}

bool HasClass(const UnitTargets& u, const std::string& c, bool ownClasses) {
	if (c == "any") return true;
	if (ownClasses) {
		if (c == "anything") return true;
		if (std::find(u.classes.begin(), u.classes.end(), c) != u.classes.end()) return true;
	}
	if (c == "command") return u.command;
	if (c == "mobile") return u.mobile;
	if (c == "defence") return u.defence;
	if (c == "constructor") return u.constructor;
	if (c == "plant") return u.plant;
	if (!ownClasses) return false;   // unit_cmd_gun.lua's CLASS table ends here
	if (c == "generator") return u.generator;
	if (c == "sensor_post") return u.sensorPost;
	if (c == "barrier") return u.barrier;
	return false;
}

// Lua Rank: the first priority group whose classes the target all has
int32_t ComputeRank(const WeaponTargets& w, const UnitTargets& u) {
	for (size_t i = 0; i < w.priorities.size(); ++i) {
		bool all = true;
		for (const std::string& c : w.priorities[i]) {
			if (!HasClass(u, c, true)) { all = false; break; }
		}
		if (all) return static_cast<int32_t>(i + 1);
	}
	return 0;
}

void BuildRanks(TargetingDefs& d) {
	d.ranks.assign(d.weapons.size(), {});
	for (size_t w = 0; w < d.weapons.size(); ++w) {
		if (!d.weapons[w].scanned) continue;
		d.ranks[w].assign(d.units.size(), 0);
		for (size_t u = 0; u < d.units.size(); ++u)
			d.ranks[w][u] = static_cast<uint8_t>(std::clamp<int32_t>(ComputeRank(d.weapons[w], d.units[u]), 0, UINT8_MAX));
	}
}

void BuildGunRanks(TargetingDefs& d, const std::vector<ClassName>& priorities) {
	d.gunRanks.assign(d.units.size(), 0);
	for (size_t u = 0; u < d.units.size(); ++u) {
		for (size_t i = 0; i < priorities.size(); ++i) {
			if (HasClass(d.units[u], priorities[i].name, false)) {
				d.gunRanks[u] = static_cast<uint8_t>(std::min<size_t>(i + 1, UINT8_MAX));
				break;
			}
		}
	}
}

// Lua LayerOK (REQ CMB-018 R1-R4, CMB-019 R3, R6)
Refusal LayerCheck(const LayerReach& w, bool targetAir, Layer layer, float y, float height) {
	if (targetAir) {
		if (w.below) return Refusal::BelowWaterOnly;
		return w.air ? Refusal::None : Refusal::NoAirTarget;
	}
	if (w.noGround) return Refusal::AirToAirOnly;
	if (layer == Layer::Unknown) layer = (y + height < 0) ? Layer::Sub : Layer::Land;   // the fallback height rule
	switch (layer) {
		case Layer::Seabed: return (!w.above && (w.seafloor || w.under)) ? Refusal::None : Refusal::TargetBelowWater;
		case Layer::Sub: return (!w.above && w.under) ? Refusal::None : Refusal::TargetBelowWater;
		case Layer::Water: return w.surface ? Refusal::None : Refusal::NoSurfaceTarget;
		default: break;
	}
	return (!w.below && w.surface) ? Refusal::None : Refusal::TargetAboveWater;
}

void TargetState::Init(int32_t maxUnits) {
	units.Init(maxUnits);
	GunColumns blank;
	blank.target.fill(NONE);
	blank.scanDue.fill(NONE);
	units.Own(guns, blank);
	units.Own(focus, NONE);
	units.Own(inFlight, 0.0f);
	units.Own(answer, AllowAnswer{});
	units.Own(gunner, CommandGunner{});
	gunners.Init(maxUnits);
	shots.clear();
}

void TargetUnitCreated(TargetState& s, const TargetingContext& c, int32_t unit, int32_t unitDef, int32_t step) {
	if (s.units.index.Has(unit)) TargetUnitDestroyed(s, unit);
	const auto slot = s.units.AddUnit(unit);
	if (slot == DenseIndex::NONE) return;
	GunColumns g;
	g.target.fill(NONE);
	g.scanDue.fill(NONE);
	s.focus[slot] = NONE;
	s.inFlight[slot] = 0;
	s.answer[slot] = AllowAnswer{};
	s.gunner[slot] = CommandGunner{};
	const UnitTargets* ud = UnitOf(c.defs, unitDef);
	if (ud != nullptr) {
		for (int32_t n = 0; n < MAX_UNIT_WEAPONS; ++n) {
			const WeaponTargets* w = WeaponOf(c.defs, ud->weapons[n]);
			if (w == nullptr || ud->weapons[n] == 0 || !w->scanned) continue;
			g.scanDue[n] = FirstScan(c.params, unit, n + 1, ScanSteps(c.params, w->scanInterval), step);
		}
		// unit_cmd_gun.lua: weapon 1 is the command gun
		if (c.defs.gunWeaponDef != NONE && ud->weapons[0] == c.defs.gunWeaponDef) {
			s.gunner[slot] = CommandGunner{true, NONE, step + c.params.gunScanSteps};
			s.gunners.Insert(unit);
		}
	}
	s.guns[slot] = g;
}

void TargetUnitDestroyed(TargetState& s, int32_t unit) {
	if (!s.units.index.Has(unit)) return;
	s.gunners.Erase(unit);
	s.units.RemoveUnit(unit);
}

void SetFocus(TargetState& s, int32_t unit, int32_t target) {
	const auto slot = s.units.index.SlotOf(unit);
	if (slot != DenseIndex::NONE) s.focus[slot] = target;
}

// Lua ProjectileCreated, after the shot's raw damage was read (ShotDamage): the share k of it is counted
bool AddShot(TargetState& s, const TargetingContext& c, int32_t projectile, int32_t weaponDef, int32_t target,
	float damage, float& total) {
	const WeaponTargets* w = WeaponOf(c.defs, weaponDef);
	if (w == nullptr || !w->scanned || !Overkill(c.params)) return false;
	const auto slot = s.units.index.SlotOf(target);
	if (slot == DenseIndex::NONE) return false;
	const float dmg = damage * c.params.overkillFactor;
	if (dmg <= 0) return false;
	const auto at = std::lower_bound(s.shots.begin(), s.shots.end(), projectile,
		[](const FlightShot& a, int32_t id) { return a.projectile < id; });
	s.shots.insert(at, FlightShot{projectile, target, dmg});
	s.inFlight[slot] = s.inFlight[slot] + dmg;
	total = s.inFlight[slot];
	return true;
}

// Lua ProjectileDestroyed
bool RemoveShot(TargetState& s, const TargetingContext& c, int32_t projectile, int32_t& target, float& total) {
	const auto at = std::lower_bound(s.shots.begin(), s.shots.end(), projectile,
		[](const FlightShot& a, int32_t id) { return a.projectile < id; });
	if (at == s.shots.end() || at->projectile != projectile) return false;
	const FlightShot shot = *at;
	s.shots.erase(at);
	target = shot.target;
	const auto slot = s.units.index.SlotOf(target);
	const float have = (slot != DenseIndex::NONE) ? s.inFlight[slot] : 0;
	const float left = have - shot.damage;
	total = (left > c.params.inflightEpsilon) ? left : 0;
	if (slot != DenseIndex::NONE) s.inFlight[slot] = total;
	return true;
}

float InFlight(const TargetState& s, int32_t target) {
	const auto slot = s.units.index.SlotOf(target);
	return (slot != DenseIndex::NONE) ? s.inFlight[slot] : 0;
}

bool Contact(TargetingWorld& world, int32_t target, int32_t allyTeam) {
	return world.LosBits(target, allyTeam) % LOS_BITS > 0;
}

// Lua Acceptable(u, w, t, noLine)
TargetTest Acceptable(const TargetingContext& c, int32_t u, int32_t weaponDef, int32_t t, bool noLine, TargetingWorld& world) {
	if (!world.ValidUnit(t) || world.DeadUnit(t)) return {};
	if (world.Allied(world.UnitTeam(u), world.UnitTeam(t))) return {};
	if (world.Arriving(t)) return {};
	if (!Contact(world, t, world.AllyTeam(u))) return {false, true};
	const WeaponTargets* w = WeaponOf(c.defs, weaponDef);
	if (w == nullptr) return {};
	if (LayerOf(w->reach, t, c, world) != Refusal::None) return {false, true};
	if (noLine || !w->direct || (LineClear(c.params, world, u, t) && FriendsClear(c, world, u, t))) return {true, false};
	return {false, true};
}

namespace {

// one weapon's rank and reach on the target, or found false (Lua: r and Acceptable(...))
Engagement WeaponEngage(const TargetingContext& c, int32_t u, int32_t n, int32_t wd, int32_t td, int32_t t, bool noLine,
	TargetingWorld& world) {
	const int32_t r = c.defs.Rank(wd, td);
	if (r == 0 || !Acceptable(c, u, wd, t, noLine, world).ok) return {};
	float range = 0;
	if (!world.WeaponRange(u, n, range)) range = 0;
	return {true, range - c.params.engageInset, r};
}

void Better(Engagement& best, const Engagement& e) {
	if (!e.found) return;
	if (!best.found || e.rank < best.rank || (e.rank == best.rank && e.reach > best.reach)) best = e;
}

const UnitTargets* GunsOf(const TargetingContext& c, int32_t u, TargetingWorld& world, int32_t& td) {
	const UnitTargets* ud = UnitOf(c.defs, world.UnitDef(u));
	td = NONE;
	if (ud == nullptr) return nullptr;
	for (int32_t n = 0; n < MAX_UNIT_WEAPONS; ++n) {
		const WeaponTargets* w = WeaponOf(c.defs, ud->weapons[n]);
		if (ud->weapons[n] != 0 && w != nullptr && w->scanned) return ud;
	}
	return nullptr;   // Lua: guns[u] is nil
}

} // namespace

Engagement Engage(const TargetingContext& c, int32_t u, int32_t t, bool noLine, int32_t only, TargetingWorld& world) {
	int32_t unused;
	const UnitTargets* ud = GunsOf(c, u, world, unused);
	if (ud == nullptr || world.FireState(u) == 0) return {};
	const int32_t td = world.UnitDef(t);
	Engagement best;
	for (int32_t n = 1; n <= MAX_UNIT_WEAPONS; ++n) {
		const int32_t wd = ud->weapons[n - 1];
		const WeaponTargets* w = WeaponOf(c.defs, wd);
		if (wd == 0 || w == nullptr || !w->scanned || (only != 0 && n != only)) continue;
		Better(best, WeaponEngage(c, u, n, wd, td, t, noLine, world));
	}
	return best;
}

Engagement2 Engage2(const TargetingContext& c, int32_t u, int32_t t, bool noLine, TargetingWorld& world) {
	int32_t unused;
	const UnitTargets* ud = GunsOf(c, u, world, unused);
	if (ud == nullptr || world.FireState(u) == 0) return {};
	const int32_t td = world.UnitDef(t);
	Engagement2 out;
	for (int32_t n = 1; n <= MAX_UNIT_WEAPONS; ++n) {
		const int32_t wd = ud->weapons[n - 1];
		const WeaponTargets* w = WeaponOf(c.defs, wd);
		if (wd == 0 || w == nullptr || !w->scanned) continue;
		const Engagement e = WeaponEngage(c, u, n, wd, td, t, noLine, world);
		if (n == 1) out.primary = e;
		Better(out.best, e);
	}
	return out;
}

bool BestKey(const TargetingContext& c, int32_t unitDef, int32_t targetDef, int32_t& kind, int32_t& rank) {
	const UnitTargets* ud = UnitOf(c.defs, unitDef);
	if (ud == nullptr) return false;
	int32_t best = 0;
	for (int32_t n = 1; n <= MAX_UNIT_WEAPONS; ++n) {
		const int32_t wd = ud->weapons[n - 1];
		const WeaponTargets* w = WeaponOf(c.defs, wd);
		if (wd == 0 || w == nullptr || !w->scanned) continue;
		const int32_t r = c.defs.Rank(wd, targetDef);
		if (r == 0) continue;
		if (n == 1) { kind = 0; rank = r; return true; }
		if (best == 0 || r < best) best = r;
	}
	if (best == 0) return false;
	kind = 1;
	rank = best;
	return true;
}

// Lua Refusal(u, defID, t, cmdID)
Refusal OrderRefusal(const TargetingContext& c, int32_t unitDef, int32_t t, bool manualFire, TargetingWorld& world) {
	const UnitTargets* ud = UnitOf(c.defs, unitDef);
	if (ud == nullptr) return Refusal::None;
	Refusal why = Refusal::None;
	bool any = false;
	for (int32_t n = 0; n < MAX_UNIT_WEAPONS; ++n) {
		const int32_t wd = ud->weapons[n];
		if (wd == 0) break;   // the def's weapons end at the first empty number
		const WeaponTargets* w = WeaponOf(c.defs, wd);
		const LayerReach* reach = nullptr;
		if (manualFire) {
			if (w != nullptr && w->manualFire) reach = &w->manualReach;
		} else {
			if (w == nullptr || !w->scanned) return Refusal::None;   // a weapon outside this system decides itself
			reach = &w->reach;
		}
		if (reach == nullptr) continue;
		any = true;
		const Refusal r = LayerOf(*reach, t, c, world);
		if (r == Refusal::None) return Refusal::None;
		if (why == Refusal::None) why = r;
	}
	if (!any) return Refusal::None;
	return (why == Refusal::None) ? Refusal::CannotTarget : why;
}

// Lua gadget:AllowCommand
bool AllowOrder(TargetState& s, const TargetingContext& c, const OrderView& o, int32_t step, TargetingWorld& world) {
	int32_t cmd = o.command;
	int32_t np = o.nparams;
	float t1 = o.params[0];
	const CommandIds& ids = c.params.commands;
	if (cmd == ids.insert && np > INSERT_WHAT) {
		cmd = static_cast<int32_t>(o.params[INSERT_WHAT]);
		np = std::clamp<int32_t>(np - INSERT_OWN, 0, INSERT_OWN_MAX);   // Lua: { params[4], params[5], params[6] }, # to the first nil
		t1 = o.params[INSERT_OWN];
	}
	if ((cmd != ids.attack && cmd != ids.manualFire) || np != 1) return true;
	const int32_t t = static_cast<int32_t>(t1);
	if (!world.ValidUnit(t) || t == o.unit) return true;
	if (world.ScriptAI(o.team)) return true;
	const Refusal why = OrderRefusal(c, o.unitDef, t, cmd == ids.manualFire, world);
	if (why == Refusal::None) return true;
	const auto slot = s.units.index.SlotOf(o.unit);
	if (slot != DenseIndex::NONE) s.answer[slot] = AllowAnswer{false, why, step};
	world.WriteRefusal(o.unit, why, step);
	return false;
}

// Lua AllowBody (cmb_targeting) and unit_cmd_gun's AllowWeaponTarget: the engine asks every gadget, so both must allow
bool AllowTarget(TargetState& s, const TargetingContext& c, int32_t attacker, int32_t target, int32_t weapon,
	int32_t weaponDef, int32_t step, TargetingWorld& world) {
	if (target < 0) return true;
	bool allow = true;
	const WeaponTargets* w = WeaponOf(c.defs, weaponDef);
	if (w != nullptr && w->scanned && world.ChosenTarget(attacker, weapon) != target) {
		const CommandView cmd = world.CurrentCommand(attacker);
		const int32_t internal = c.params.commands.internalOption;
		const bool order = cmd.command == c.params.commands.attack && cmd.nparams == 1
			&& static_cast<int32_t>(cmd.p1) == target && !Internal(cmd.options, internal);
		allow = order;
	}
	if (allow && weaponDef == c.defs.gunWeaponDef) {
		const auto slot = s.units.index.SlotOf(attacker);
		allow = slot != DenseIndex::NONE && s.gunner[slot].present && s.gunner[slot].target == target;
	}
	const auto slot = s.units.index.SlotOf(attacker);
	if (slot != DenseIndex::NONE) s.answer[slot] = AllowAnswer{allow, Refusal::None, step};
	return allow;
}

namespace {

// unit_cmd_gun.lua Acceptable: alive, an enemy, not arriving, in sight, not submerged (CMD-004 v3 R6)
bool GunAcceptable(TargetingWorld& world, int32_t u, int32_t t) {
	if (!world.ValidUnit(t) || world.DeadUnit(t)) return false;
	if (world.Allied(world.UnitTeam(u), world.UnitTeam(t))) return false;
	if (world.Arriving(t)) return false;
	if (!world.InLos(t, world.AllyTeam(u))) return false;
	float pos[3];
	return world.Position(t, pos) && pos[1] >= 0;
}

float GunRangeMax(const TargetingParams& p, TargetingWorld& world, int32_t u) {
	float range = 0;
	return world.WeaponRange(u, 1, range) ? range : p.gunRangeMax;
}

int32_t GunScan(const TargetingContext& c, TargetingWorld& world, int32_t u) {
	float x[3];
	if (!world.Position(u, x)) return NONE;
	const float reach = GunRangeMax(c.params, world, u) - c.params.gunInset;
	std::vector<int32_t> near;
	world.UnitsInCylinder(x[0], x[2], reach, near);
	int32_t best = NONE, bestRank = 0;
	float bestDist = 0;
	for (const int32_t t : near) {   // in the engine's order, as Spring.GetUnitsInCylinder gives it: ties keep the first
		const int32_t td = world.UnitDef(t);
		const int32_t r = (td >= 0 && static_cast<size_t>(td) < c.defs.gunRanks.size()) ? c.defs.gunRanks[td] : 0;
		if (r == 0 || !GunAcceptable(world, u, t)) continue;
		float d = 0;
		if (!HDistUnits(world, u, t, d)) d = FAR;
		if (d <= reach && d >= c.params.gunRangeMin && (best == NONE || r < bestRank || (r == bestRank && d < bestDist))) {
			best = t; bestRank = r; bestDist = d;
		}
	}
	return best;
}

} // namespace

void RunCommandGuns(TargetState& s, const TargetingContext& c, int32_t step, TargetingWorld& world) {
	const CommandIds& ids = c.params.commands;
	for (const int32_t u : s.gunners.Ids()) {
		const auto slot = s.units.index.SlotOf(u);
		if (slot == DenseIndex::NONE) continue;
		CommandGunner& g = s.gunner[slot];
		if (g.target != NONE) {
			float d = 0;
			if (!HDistUnits(world, u, g.target, d)) d = FAR;
			if (!GunAcceptable(world, u, g.target) || d > GunRangeMax(c.params, world, u) * c.params.gunTrack) {
				g.target = NONE;
				g.nextScan = step;
			}
		}
		const CommandView cmd = world.CurrentCommand(u);
		const int32_t internal = ids.internalOption;
		const bool attackOrder = (cmd.command == ids.attack || cmd.command == ids.manualFire)
			&& !Internal(cmd.options, internal);
		if (attackOrder || world.FireState(u) == 0 || world.Arriving(u)) {
			g.target = NONE;
		} else if (g.target == NONE && step >= g.nextScan) {
			g.target = GunScan(c, world, u);
			g.nextScan = step + c.params.gunScanSteps;
		}
		if (g.target != NONE && world.WeaponTarget(u, 1) != g.target) world.SetWeaponTarget(u, 1, g.target);
	}
}

} // namespace KT1::Core::Targeting
