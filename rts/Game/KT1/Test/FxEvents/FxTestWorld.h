/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 FxEvents tests: a hand-made world and the synced effect system around it.

#ifndef KT1_TEST_FX_TEST_WORLD_H
#define KT1_TEST_FX_TEST_WORLD_H

#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "Game/KT1/Core/Common/Rate.h"
#include "Game/KT1/Core/FxEvents/FxLogic.h"

namespace KT1::Test::Fx {

using namespace KT1::Core::Fx;

struct FakeWorld {
	std::map<int32_t, UnitNow> units;
	float ground = 10.0f;               // DESIGN: flat ground
	bool near = false;
	SeenMask mask = 5;                  // DESIGN: ally teams 0 and 2 see everything
	float draw = 0.5f;                  // DESIGN: every random() answers this
	int draws = 0;
	std::vector<std::pair<int32_t, int32_t>> ranges;

	static FakeWorld& Of(void* ctx) { return *static_cast<FakeWorld*>(ctx); }

	WorldQuery Query() {
		WorldQuery q;
		q.ctx = this;
		q.groundHeight = [](void* c, float, float) { return Of(c).ground; };
		q.groundNormal = [](void*, float, float, Vec3& n) { n = {0.0f, 1.0f, 0.0f}; };
		q.unit = [](void* c, int32_t id, UnitNow& out) {
			const auto it = Of(c).units.find(id);
			if (it == Of(c).units.end()) return false;
			out = it->second;
			return true;
		};
		q.featurePos = [](void*, int32_t, Vec3&) { return false; };
		q.featureMass = [](void*, int32_t) { return 0.0f; };
		q.unitsNear = [](void* c, const Vec3&, float) { return Of(c).near; };
		q.seen = [](void* c, const Vec3&) { return Of(c).mask; };
		q.random = [](void* c) { ++Of(c).draws; return Of(c).draw; };
		q.randomRange = [](void* c, int32_t lo, int32_t hi) { Of(c).ranges.push_back({lo, hi}); return lo; };
		q.mapSizeX = 4096.0f;           // DESIGN
		q.mapSizeZ = 4096.0f;           // DESIGN
		return q;
	}
};

struct Rig {
	FxEventsConfig config;
	FxDefs defs;
	FxState state;
	FxOutput out;
	FakeWorld world;
	WorldQuery query = world.Query();

	void Row(const char* on, const char* display) {
		FxEventRow row;
		row.on = on;
		row.display_event = display;
		config.events.push_back(row);
	}
	void AllRows() {
		for (const char* d: {"Scorch", "DepthHit", "Blast", "ScriptBlast", "Impact"}) Row("Explosion", d);
		for (const char* d: {"Death", "FogDeath", "CritDeath", "Blast", "Scorch"}) Row("UnitDestroyed", d);
		for (const char* d: {"ShotStart", "DepthLaunch"}) Row("ProjectileCreated", d);
		Row("ProjectileDestroyed", "ShotEnd");
	}
	void Init(float scale = 1.0f) { InitFx(state, config, {64, 64, 8}, scale); }
	FxEnv Env(int32_t step) { return {config, defs, query, state, out, step}; }

	// the events' types as offsets from EVENT_BASE, in order
	std::vector<int> Types() const {
		std::vector<int> t;
		for (const auto& e: out.events) t.push_back(e.id - EVENT_BASE);
		return t;
	}
	// the n-th event of type T
	template<class T> T Get(int n = 0) const {
		T found{};
		for (const auto& e: out.events) {
			T payload;
			if (e.Payload(payload) && n-- == 0) return payload;
		}
		return found;
	}
	template<class T> int Count() const {
		int n = 0;
		for (const auto& e: out.events)
			if (e.id == T::TYPE) ++n;
		return n;
	}
};

template<class T> int Kind() { return T::TYPE - EVENT_BASE; }
inline EffectRef Ref(int32_t id) { return {id, id, false}; }

} // namespace KT1::Test::Fx

#endif
