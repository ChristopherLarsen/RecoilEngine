/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 EFFECTS core: wreck physics (replaces code/LuaRules/Gadgets/fx_wreck_physics.lua; the names in the comments are its
// functions). The order of events, writes and releases and the order of the float operations are the Lua's. No random draw.
//
// Column wreck_impulse (WreckState::impulse): the push (vx, up, vz) a wreck in motion was given in the step last run.
// ExplodeNearWrecks clears it at the first explosion it looks at in a step, StepWrecks in a step without one, so after
// StepWrecks it holds that step's explosion pushes.

#include "Game/KT1/Core/FxEvents/FxWreck.h"

#include <algorithm>
#include <cmath>

#include "Game/KT1/Core/Common/Rate.h"
#include "Game/KT1/Core/FxEvents/FxState.h"

namespace KT1::Core::Fx {

namespace {

constexpr float SLOPE_EPS = 1e-6f;       // PLACEHOLDER: Slope: a flatter normal has no downhill direction
constexpr float CENTRE_EPS = 1e-3f;      // PLACEHOLDER: nearer the centre than this the push goes along x
constexpr float WHOLE_EPS = 1e-4f;       // DESIGN: a period count this near a whole number is that number
constexpr int ROOT_DIGITS = 24;          // DESIGN: binary digits taken of a part period (a float's mantissa)
constexpr int32_t SPIN_X_CYCLE = 3;      // PLACEHOLDER: the placing numbers after which the x spin share repeats
constexpr int32_t SPIN_Y_CYCLE = 5;      // PLACEHOLDER: the same for y
constexpr int32_t SPIN_Z_CYCLE = 2;      // PLACEHOLDER: the same for z
constexpr int32_t PARITY_CYCLE = 2;      // DESIGN: even or odd feature id (a wreck at the very centre)

// share^periods with + * and sqrt only (no pow: it differs between C libraries). One period is the share itself.
float ShareOver(float share, float periods) {
	const float nearest = std::floor(periods + static_cast<float>(1) / 2);   // DESIGN: half, to the nearest whole
	if (std::fabs(periods - nearest) < WHOLE_EPS)
		periods = nearest;
	const int whole = static_cast<int>(std::floor(periods));
	float result = 1.0f;
	for (int i = 0; i < whole; ++i)
		result = result * share;
	float part = periods - static_cast<float>(whole);
	float root = std::max(0.0f, share);
	for (int i = 0; i < ROOT_DIGITS && part > 0.0f; ++i) {
		root = std::sqrt(root);
		part = part + part;
		if (part >= 1.0f) {
			result = result * root;
			part = part - 1.0f;
		}
	}
	return result;
}

void RemoveMoving(WreckState& s, int32_t feature) {
	const DenseIndex::Move move = s.moving.Remove(feature);   // the last one takes this place, as the Lua's Drop
	s.impulse.Apply(move);
	s.vel.Apply(move);
	s.rot.Apply(move);
	s.spin.Apply(move);
	s.air.Apply(move);
	s.tilted.Apply(move);
}

void RemovePlaced(WreckState& s, int32_t feature) {
	const DenseIndex::Move move = s.placed.Remove(feature);
	s.seq.Apply(move);
	s.pose.Apply(move);
	s.posed.Apply(move);
	s.thudAt.Apply(move);
}

// a slot of `placed` may hold only a pose or a thud's step; the wreck may be moved by explosions once it has a number
bool Alive(const WreckState& s, int32_t feature) {
	const DenseIndex::Slot slot = s.placed.SlotOf(feature);
	return slot != DenseIndex::NONE && s.seq[slot] != 0;
}

bool AnyAlive(const WreckState& s) {
	for (DenseIndex::Slot slot = 0; slot < s.placed.Size(); ++slot) {
		if (s.seq[slot] != 0)
			return true;
	}
	return false;
}

bool Posed(const WreckState& s, int32_t feature) {
	const DenseIndex::Slot slot = s.placed.SlotOf(feature);
	return slot != DenseIndex::NONE && s.posed[slot] != 0;
}

void ClearImpulses(WreckState& s) {
	for (Vec3& v: s.impulse.Live(s.moving))
		v = Vec3{};
}

// Start: the wreck's slot among those in motion; NONE while the list is full
DenseIndex::Slot Start(WreckState& s, const WreckConfig& c, int32_t feature) {
	const DenseIndex::Slot slot = s.moving.SlotOf(feature);
	if (slot != DenseIndex::NONE)
		return slot;
	if (s.moving.Size() >= c.max_moving)
		return DenseIndex::NONE;
	return s.moving.Add(feature);
}

// Slope: the horizontal part of the ground normal, and the downhill direction
float Slope(const WorldQuery& world, float x, float z, float& dx, float& dz) {
	Vec3 n;
	world.groundNormal(world.ctx, x, z, n);
	const float hn = std::sqrt(n.x * n.x + n.z * n.z);
	dx = (hn > SLOPE_EPS) ? n.x / hn : 0.0f;
	dz = (hn > SLOPE_EPS) ? n.z / hn : 0.0f;
	return hn;
}

float Ease(float a, float keep, float zero) {
	a = a * keep;
	if (std::fabs(a) < zero)
		return 0.0f;
	return a;
}

void Thud(WreckState& s, const WreckParams& p, int32_t feature, const Vec3& pos, float speed, int32_t step, FxOutput& out) {
	if (speed < p.thudMinDrop)
		return;
	if (s.lastThud >= 0 && step - s.lastThud < p.thudGap)
		return;
	const DenseIndex::Slot known = s.placed.SlotOf(feature);
	if (known != DenseIndex::NONE && s.thudAt[known] >= 0 && step - s.thudAt[known] < p.thudWreckGap)
		return;
	s.lastThud = step;
	const DenseIndex::Slot slot = s.placed.Add(feature);
	if (slot != DenseIndex::NONE)
		s.thudAt[slot] = step;
	out.Raise(WreckThud{feature, pos, speed});
}

// Step, in the air: gravity and spin; down to the ground it bounces or lands
void StepAir(WreckState& s, const WreckConfig& c, const WreckParams& p, DenseIndex::Slot i, Vec3& pos, int32_t step,
	const WorldQuery& world, FxOutput& out) {
	Vec3& v = s.vel[i];
	Vec3& r = s.rot[i];
	Vec3& w = s.spin[i];
	v.y = v.y - world.gravity;
	pos.x = pos.x + v.x;
	pos.y = pos.y + v.y;
	pos.z = pos.z + v.z;
	r.x = r.x + w.x;
	r.y = r.y + w.y;
	r.z = r.z + w.z;
	const float gy = world.groundHeight(world.ctx, pos.x, pos.z);
	if (!(pos.y <= gy)) {
		s.air[i] = 1;
		return;
	}
	pos.y = gy;
	Thud(s, p, s.moving.IdOf(i), pos, -v.y, step, out);
	if (v.y < -p.bounceMin) {
		v.y = -v.y * c.bounce;
		v.x = v.x * c.bounce_side;
		v.z = v.z * c.bounce_side;
		w.x = w.x * c.bounce_spin;
		w.y = w.y * c.bounce_spin;
		w.z = w.z * c.bounce_spin;
	} else {
		s.air[i] = 0;
		v.y = 0.0f;
		w = Vec3{0.0f, 0.0f, 0.0f};
	}
}

// Step, on the ground: slides while the slope is steeper than its friction, else slows; the tilt eases back
void StepGround(WreckState& s, const WreckConfig& c, const WreckParams& p, DenseIndex::Slot i, Vec3& pos, float gy,
	const WorldQuery& world) {
	Vec3& v = s.vel[i];
	Vec3& r = s.rot[i];
	float dx = 0.0f, dz = 0.0f;
	const float hn = Slope(world, pos.x, pos.z, dx, dz);
	const float speed = std::sqrt(v.x * v.x + v.z * v.z);
	if (hn > c.kinetic_friction && (hn > c.static_friction || speed > p.stopSpeed)) {
		const float a = world.gravity * (hn - c.kinetic_friction);
		v.x = v.x + dx * a;
		v.z = v.z + dz * a;
	}
	const float drag = (gy < 0.0f) ? p.waterDrag : p.drag;
	v.x = v.x * drag;
	v.z = v.z * drag;
	pos.x = pos.x + v.x;
	pos.z = pos.z + v.z;
	pos.y = world.groundHeight(world.ctx, pos.x, pos.z);
	r.x = Ease(r.x, p.groundEase, c.ground_ease_zero);
	r.y = Ease(r.y, p.groundEase, c.ground_ease_zero);
	r.z = Ease(r.z, p.groundEase, c.ground_ease_zero);
}

// the height is not read again at the clamped place (as the Lua)
void ClampToMap(const WreckConfig& c, const WorldQuery& world, Vec3& pos, Vec3& vel) {
	const float m = c.edge_margin;
	if (pos.x < m) {
		pos.x = m;
		vel.x = 0.0f;
	} else if (pos.x > world.mapSizeX - m) {
		pos.x = world.mapSizeX - m;
		vel.x = 0.0f;
	}
	if (pos.z < m) {
		pos.z = m;
		vel.z = 0.0f;
	} else if (pos.z > world.mapSizeZ - m) {
		pos.z = world.mapSizeZ - m;
		vel.z = 0.0f;
	}
}

// the new place, and the rotation while tilted or spinning and once more when it comes flat
void Write(WreckState& s, DenseIndex::Slot i, const Vec3& pos, FxOutput& out) {
	const Vec3& r = s.rot[i];
	const Vec3& w = s.spin[i];
	const bool tilted = r.x != 0.0f || r.y != 0.0f || r.z != 0.0f || w.x != 0.0f || w.y != 0.0f || w.z != 0.0f;
	WreckWrite write;
	write.feature = s.moving.IdOf(i);
	write.pos = pos;
	if (tilted || s.tilted[i] != 0) {
		write.setRotation = true;
		write.rot = r;
		if (Posed(s, write.feature)) {   // lying as its death left it
			const Vec3& b = s.pose[s.placed.SlotOf(write.feature)];
			write.rot = Vec3{b.x + r.x, b.y + r.y, b.z + r.z};
		}
	}
	s.tilted[i] = tilted ? 1 : 0;
	out.wreckWrites.push_back(write);
}

// at rest: on the ground, nearly still, flat, on ground it can rest on
bool AtRest(const WreckState& s, const WreckConfig& c, const WreckParams& p, DenseIndex::Slot i, const Vec3& pos,
	const WorldQuery& world) {
	if (s.air[i] != 0)
		return false;
	const Vec3& v = s.vel[i];
	const Vec3& r = s.rot[i];
	const float speed = std::sqrt(v.x * v.x + v.z * v.z);
	float dx = 0.0f, dz = 0.0f;
	const float hn = Slope(world, pos.x, pos.z, dx, dz);
	return speed < p.stopSpeed && hn <= c.static_friction && r.x == 0.0f && r.y == 0.0f && r.z == 0.0f;
}

// Step: one wreck in motion, from `pos`; false: it has come to rest
bool StepOne(WreckState& s, const WreckConfig& c, const WreckParams& p, DenseIndex::Slot i, Vec3 pos, int32_t step,
	const WorldQuery& world, FxOutput& out) {
	const float gy = world.groundHeight(world.ctx, pos.x, pos.z);
	if (s.air[i] != 0 || pos.y > gy + c.air_height)
		StepAir(s, c, p, i, pos, step, world, out);
	else
		StepGround(s, c, p, i, pos, gy, world);
	ClampToMap(c, world, pos, s.vel[i]);
	Write(s, i, pos, out);
	return !AtRest(s, c, p, i, pos, world);
}

float Dist2To(const WreckNear& n, const Vec3& at) {
	const float dx = n.pos.x - at.x, dz = n.pos.z - at.z;
	return dx * dx + dz * dz;
}

// the push one explosion gives one wreck, nearer and lighter ones harder
void PushFrom(WreckState& s, const WreckConfig& c, const WreckParams& p, const WreckPushWeapon& w, int32_t weaponDef,
	const Vec3& at, const WreckNear& e, const WorldQuery& world, FxOutput& out) {
	const float dmul = std::min(c.push_max_damage_mul, w.damage / c.push_ref_damage);
	const float ex = e.pos.x - at.x, ez = e.pos.z - at.z;
	const float d = std::sqrt(Dist2To(e, at));
	const float share = 1.0f - d / w.radius;   // 1 at the centre, 0 at the edge
	if (!(share > 0.0f))
		return;
	const float mass = world.featureMass(world.ctx, e.feature);
	const float v = p.pushSpeed * dmul * share / (1.0f + mass / c.push_mass_ref);
	float ux = (e.feature % PARITY_CYCLE == 0) ? 1.0f : -1.0f, uz = 0.0f;
	if (d > CENTRE_EPS) {
		ux = ex / d;
		uz = ez / d;
	}
	float up = 0.0f, spin = 0.0f;
	if (w.tumbles && share >= c.tumble_min_share) {
		up = p.upSpeed * share * dmul;
		spin = p.spin * share;
	}
	const float vx = ux * v, vz = uz * v;
	if (PushWreck(s, c, e.feature, vx, vz, up, spin))
		out.Raise(WreckImpulse{e.feature, weaponDef, e.pos, vx, vz, up, spin});
}

} // namespace

WreckParams MakeWreckParams(const WreckConfig& c) {
	const float sps = static_cast<float>(Rate::StepsPerSecond());
	// the steps' share of drag_step_s, as a count of such periods (1 at the Lua's rate)
	const float periods = (c.drag_step_s > 0.0f) ? Rate::StepSeconds() / c.drag_step_s : 1.0f;
	WreckParams p;
	p.drag = ShareOver(c.drag, periods);
	p.waterDrag = ShareOver(c.water_drag, periods);
	p.groundEase = ShareOver(c.ground_ease, periods);
	// divided, not multiplied by the step length: the quotient is the float nearest the Lua's per frame value
	p.stopSpeed = c.stop_speed / sps;
	p.pushSpeed = c.push_max_speed / sps;
	p.upSpeed = c.tumble_up_speed / sps;
	p.spin = c.tumble_spin / sps;
	p.bounceMin = c.bounce_min / sps;
	p.thudMinDrop = c.thud_min_drop / sps;
	p.thudGap = Rate::Steps(c.thud_gap_s);
	p.thudWreckGap = Rate::Steps(c.thud_wreck_gap_s);
	return p;
}

void InitWrecks(WreckState& s, int32_t maxFeatures) {
	s.placed.Init(maxFeatures);
	s.seq.Init(s.placed);
	s.pose.Init(s.placed);
	s.posed.Init(s.placed);
	s.thudAt.Init(s.placed, -1);
	s.moving.Init(maxFeatures);
	s.impulse.Init(s.moving);
	s.vel.Init(s.moving);
	s.rot.Init(s.moving);
	s.spin.Init(s.moving);
	s.air.Init(s.moving);
	s.tilted.Init(s.moving);
	s.nextSeq = 0;
	s.lastThud = -1;
	s.budgetStep = -1;
	s.budget = 0;
}

// areaOfEffect: what the Lua reads (WeaponDefs damageAreaOfEffect); it takes half of it as the radius
WreckPushWeapon PushWeaponOf(const WreckConfig& c, float damage, float areaOfEffect) {
	const float r = areaOfEffect / 2;   // DESIGN: the Lua's half
	if (!c.enabled || !(damage >= c.push_min_damage && r >= c.push_min_radius))
		return {};
	return WreckPushWeapon{damage, r, true, damage >= c.tumble_min_damage};
}

void PlaceWreck(WreckState& s, const WreckConfig& c, int32_t feature, const WorldQuery& world) {
	if (!c.enabled)
		return;
	const DenseIndex::Slot slot = s.placed.Add(feature);
	if (slot == DenseIndex::NONE)
		return;
	s.seq[slot] = ++s.nextSeq;
	// CheckSlope: starts to slide if the ground is steeper than it can rest on
	Vec3 pos;
	if (!world.featurePos(world.ctx, feature, pos))
		return;
	float dx = 0.0f, dz = 0.0f;
	if (Slope(world, pos.x, pos.z, dx, dz) > c.static_friction)
		Start(s, c, feature);
}

// FeatureDestroyed. The wreck layer drops its pose before this gadget's Drop in the Lua, so a posed one is released too.
void ForgetWreck(WreckState& s, int32_t feature, FxOutput& out) {
	if (s.moving.Has(feature)) {
		out.releaseWrecks.push_back(feature);
		RemoveMoving(s, feature);
	}
	RemovePlaced(s, feature);
}

bool PushWreck(WreckState& s, const WreckConfig& c, int32_t feature, float vx, float vz, float up, float spin) {
	if (!c.enabled)
		return false;
	const DenseIndex::Slot i = Start(s, c, feature);
	if (i == DenseIndex::NONE)
		return false;
	Vec3& v = s.vel[i];
	v.x = v.x + vx;
	v.z = v.z + vz;
	s.impulse[i].x += vx;
	s.impulse[i].z += vz;
	if (up > 0.0f) {
		v.y = v.y + up;
		s.air[i] = 1;
		s.impulse[i].y += up;
		const float k = c.axis_step;
		const DenseIndex::Slot placed = s.placed.SlotOf(feature);
		const int32_t q = (placed != DenseIndex::NONE) ? s.seq[placed] : 0;
		// the order it was placed in picks the axes' shares
		s.spin[i] = Vec3{spin * (1.0f + static_cast<float>(q % SPIN_X_CYCLE) * k),
			spin * (1.0f + static_cast<float>(q % SPIN_Y_CYCLE) * k),
			spin * (1.0f + static_cast<float>(q % SPIN_Z_CYCLE) * k)};
	}
	return true;
}

bool WreckMoving(const WreckState& s, int32_t feature) { return s.moving.Has(feature); }

void SetWreckPose(WreckState& s, int32_t feature, const Vec3& rot) {
	const DenseIndex::Slot slot = s.placed.Add(feature);
	if (slot == DenseIndex::NONE)
		return;
	s.pose[slot] = rot;
	s.posed[slot] = 1;
}

void ClearWreckPose(WreckState& s, int32_t feature) {
	const DenseIndex::Slot slot = s.placed.SlotOf(feature);
	if (slot == DenseIndex::NONE)
		return;
	s.pose[slot] = Vec3{};
	s.posed[slot] = 0;
	if (s.seq[slot] == 0 && s.thudAt[slot] < 0)
		RemovePlaced(s, feature);
}

void ExplodeNearWrecks(WreckState& s, const WreckConfig& c, const WreckParams& p, const WreckPushWeapon& w, int32_t weaponDef,
	const Vec3& at, std::vector<WreckNear>& near, int32_t step, const WorldQuery& world, FxOutput& out) {
	if (!c.enabled || !w.moves || !AnyAlive(s))
		return;
	if (s.budgetStep != step) {
		s.budgetStep = step;
		s.budget = c.explosions_a_step;
		ClearImpulses(s);
	}
	if (s.budget <= 0)
		return;
	--s.budget;
	std::erase_if(near, [&](const WreckNear& n) { return !Alive(s, n.feature); });
	// nearest first, then the lowest id
	std::sort(near.begin(), near.end(), [&](const WreckNear& a, const WreckNear& b) {
		const float da = Dist2To(a, at), db = Dist2To(b, at);
		if (da != db)
			return da < db;
		return a.feature < b.feature;
	});
	const int32_t count = std::min(static_cast<int32_t>(near.size()), c.per_explosion);
	for (int32_t n = 0; n < count; ++n)
		PushFrom(s, c, p, w, weaponDef, at, near[n], world, out);
}

void StepWrecks(WreckState& s, const WreckConfig& c, const WreckParams& p, int32_t step, const WorldQuery& world, FxOutput& out) {
	if (s.budgetStep != step)
		ClearImpulses(s);
	for (DenseIndex::Slot i = 0; i < s.moving.Size();) {
		const int32_t feature = s.moving.IdOf(i);
		Vec3 pos;
		if (!world.featurePos(world.ctx, feature, pos)) {
			RemoveMoving(s, feature);   // gone: nothing to release; the last one takes this place, look at it next
		} else if (!StepOne(s, c, p, i, pos, step, world, out)) {
			// Drop: back to the engine, unless it lies as its death left it
			if (!Posed(s, feature))
				out.releaseWrecks.push_back(feature);
			RemoveMoving(s, feature);
		} else {
			++i;
		}
	}
}

} // namespace KT1::Core::Fx
