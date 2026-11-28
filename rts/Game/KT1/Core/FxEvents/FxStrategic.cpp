/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 EFFECTS core: the missile relay (the synced half of fx_strategic.lua), in its order: new missiles, redirections,
// interceptors (new ones and positions, then the ended ones, newest first), tracked missiles newest first. A record
// first seen already ended was never in the Lua's list and is ignored. No engine include.

#include "Game/KT1/Core/FxEvents/FxStrategic.h"

#include <algorithm>
#include <cmath>

#include "Game/KT1/Core/FxEvents/FxState.h"

namespace KT1::Core::Fx {

namespace {

struct RelayEnv {
	StrategicState& s;
	const StrategicConfig& c;
	int32_t step;
	const WorldQuery& world;
	FxOutput& out;
};

constexpr uint8_t RESULT_HIT = 1;            // DESIGN: the wire value of the end events
constexpr uint8_t RESULT_OTHER = 2;          // DESIGN: intercepted, shot down, or its missile ended first

TrackedMissile* Tracked(StrategicState& s, int32_t key) {
	for (TrackedMissile& t : s.tracked)
		if (t.key == key) return &t;
	return nullptr;
}

int32_t IdOf(StrategicState& s, int32_t key) {
	const TrackedMissile* t = Tracked(s, key);
	return t != nullptr ? t->id : 0;
}

SeenMask SeenGround(const RelayEnv& env, float x, float z) {
	return env.world.seen(env.world.ctx, {x, env.world.groundHeight(env.world.ctx, x, z), z});
}

void Launch(const RelayEnv& env, int32_t id, const MissileIn& m) {
	StrategicLaunch e;
	e.id = id;
	e.team = m.team;
	e.allyTeam = m.allyTeam;
	e.owner = m.owner;
	e.sx = m.sx;
	e.sz = m.sz;
	e.x = m.x;
	e.z = m.z;
	e.orderStep = m.orderStep;
	e.launchStep = m.launchStep;
	e.hitStep = m.hitStep;
	e.interceptStep = m.interceptStep;
	e.defenceUnit = m.defenceUnit;
	e.dx = m.defenceX;
	e.dz = m.defenceZ;
	e.defenceAlly = m.defenceAlly;
	e.ringElmos = m.outerRadiusUnits * env.c.map_unit;
	env.out.Raise(e);
}

void NewMissiles(const RelayEnv& env, std::span<const MissileIn> missiles) {
	for (const MissileIn& m : missiles) {
		if (m.end != MissileEnd::Flying || Tracked(env.s, m.key) != nullptr) continue;
		TrackedMissile t;
		t.key = m.key;
		t.id = ++env.s.nextId;
		t.strategic = m.strategic;
		t.x = m.x;
		t.z = m.z;
		env.s.tracked.push_back(t);
		if (m.strategic != 0) Launch(env, t.id, m);
	}
}

void Redirections(const RelayEnv& env, std::span<const RedirectIn> redirects) {
	for (const RedirectIn& r : redirects) {
		MissileRedirected e;
		e.id = IdOf(env.s, r.missileKey);
		UnitNow redirector;
		if (env.world.unit(env.world.ctx, r.redirectorUnit, redirector)) {
			e.redirectorX = redirector.pos.x;
			e.redirectorZ = redirector.pos.z;
		}
		e.x = r.x;
		e.z = r.z;
		e.fromX = r.fromX;
		e.fromZ = r.fromZ;
		e.toX = r.toX;
		e.toZ = r.toZ;
		e.seenRedirector = SeenGround(env, e.redirectorX, e.redirectorZ);
		e.seenPoint = SeenGround(env, r.x, r.z);
		env.out.Raise(e);
	}
}

void Interceptors(const RelayEnv& env, std::span<const InterceptorIn> interceptors) {
	std::vector<int32_t>& known = env.s.interceptors;
	for (const InterceptorIn& i : interceptors) {
		if (i.end != MissileEnd::Flying) continue;
		if (std::find(known.begin(), known.end(), i.id) == known.end()) {
			known.push_back(i.id);
			StrategicInterceptor e;
			e.id = i.id;
			e.missile = IdOf(env.s, i.targetKey);
			e.defenceAlly = i.allyTeam;
			e.sx = i.sx;
			e.sz = i.sz;
			e.launchStep = i.launchStep;
			env.out.Raise(e);
		}
		env.out.Raise(StrategicInterceptorPos{i.id, i.x, i.z, env.step});
	}
	for (size_t k = known.size(); k > 0; --k) {
		const int32_t id = known[k - 1];
		const auto i = std::find_if(interceptors.begin(), interceptors.end(),
			[&](const InterceptorIn& in) { return in.id == id; });
		if (i == interceptors.end() || i->end == MissileEnd::Flying) continue;
		const uint8_t result = i->end == MissileEnd::Hit ? RESULT_HIT : RESULT_OTHER;
		env.out.Raise(StrategicInterceptorEnd{id, result, i->x, i->z, i->lastStep > 0 ? i->lastStep : env.step});
		known.erase(known.begin() + static_cast<std::ptrdiff_t>(k - 1));
	}
}

// a tactical missile in flight: its ground position, progress and path length
void TacticalFlight(const RelayEnv& env, TrackedMissile& t, const MissileIn& m) {
	const bool followed = t.launched != 0;       // it has a progress from an earlier step
	if (!followed) {
		t.launched = 1;
		TacticalLaunch e;
		e.id = t.id;
		e.allyTeam = m.allyTeam;
		e.sx = m.sx;
		e.sz = m.sz;
		e.x = m.x;
		e.z = m.z;
		e.seenLaunch = SeenGround(env, m.sx, m.sz);
		env.out.Raise(e);
	}
	const float span = static_cast<float>(std::max(1, m.hitStep - m.launchStep));
	float p = std::max(0.0f, std::min(1.0f, static_cast<float>(env.step - m.launchStep) / span));
	const float dx = m.x - m.sx, dz = m.z - m.sz;
	float length = std::sqrt(dx * dx + dz * dz);
	// SPC-011: a redirected missile carries on from the progress and path length it had at the turn
	if (m.redirected != 0 && t.turned == 0 && followed) {
		t.turned = 1;
		t.turnProgress = t.progress;
		t.turnLength = t.pathLength;
	}
	if (t.turned != 0) {
		p = t.turnProgress + (1 - t.turnProgress) * p;
		length = t.turnLength;
	}
	t.progress = p;
	t.pathLength = length;
	TacticalPos e;
	e.id = t.id;
	e.allyTeam = m.allyTeam;
	e.gx = m.gx;
	e.gz = m.gz;
	e.progress = p;
	e.pathLength = length;
	e.seenMissile = SeenGround(env, m.gx, m.gz);
	if (m.hasShot != 0) {
		e.shot = 1;
		e.shotX = m.shotX;
		e.shotZ = m.shotZ;
		e.shotFromX = m.shotFromX;
		e.shotFromZ = m.shotFromZ;
		e.seenShot = SeenGround(env, m.shotX, m.shotZ);
	}
	env.out.Raise(e);
	t.gx = m.gx;
	t.gz = m.gz;
}

void MissileEnded(const RelayEnv& env, const TrackedMissile& t, const MissileIn& m) {
	if (m.strategic == 0 && t.launched != 0) {
		TacticalEnd e;
		e.id = t.id;
		e.result = m.end == MissileEnd::ShotDown ? RESULT_OTHER : RESULT_HIT;
		e.x = m.hasPos != 0 ? m.gx : t.gx;
		e.z = m.hasPos != 0 ? m.gz : t.gz;
		if (m.hasShot != 0) {
			e.shotFromX = m.shotFromX;
			e.shotFromZ = m.shotFromZ;
		}
		e.seen = SeenGround(env, e.x, e.z);
		env.out.Raise(e);
	}
	if (m.strategic != 0) {
		StrategicEnd e;
		e.id = t.id;
		e.result = m.end == MissileEnd::Hit ? RESULT_HIT : RESULT_OTHER;
		e.step = m.endStep > 0 ? m.endStep : env.step;
		e.x = m.hasInterceptPos != 0 ? m.interceptX : m.x;
		e.z = m.hasInterceptPos != 0 ? m.interceptZ : m.z;
		e.ringElmos = m.outerRadiusUnits * env.c.map_unit;
		e.bands = m.outerSteps;
		e.spreadS = m.outerSpreadS;
		env.out.Raise(e);
	} else if (m.end == MissileEnd::Hit) {
		env.out.Raise(StrategicTactical{m.x, m.z});
	}
}

void TrackedMissiles(const RelayEnv& env, std::span<const MissileIn> missiles) {
	std::vector<TrackedMissile>& tracked = env.s.tracked;
	for (size_t k = tracked.size(); k > 0; --k) {
		TrackedMissile& t = tracked[k - 1];
		const auto m = std::find_if(missiles.begin(), missiles.end(), [&](const MissileIn& in) { return in.key == t.key; });
		if (m == missiles.end()) continue;
		if (m->end == MissileEnd::Flying) {
			if (m->strategic == 0 && m->hasPos != 0) TacticalFlight(env, t, *m);
			continue;
		}
		MissileEnded(env, t, *m);
		tracked.erase(tracked.begin() + static_cast<std::ptrdiff_t>(k - 1));
	}
}

} // namespace

void RelayStrategic(StrategicState& s, const StrategicConfig& c, std::span<const MissileIn> missiles,
	std::span<const InterceptorIn> interceptors, std::span<const RedirectIn> redirects, int32_t step, const WorldQuery& world,
	FxOutput& out) {
	const RelayEnv env{s, c, step, world, out};
	NewMissiles(env, missiles);
	Redirections(env, redirects);
	Interceptors(env, interceptors);
	TrackedMissiles(env, missiles);
}

} // namespace KT1::Core::Fx
