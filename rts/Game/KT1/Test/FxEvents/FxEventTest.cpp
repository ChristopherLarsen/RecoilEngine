/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// Core/FxEvents/FxEvent.h: every display event goes through FxOutput and comes back byte for byte.

#include "../KT1Test.h"

#include <cstring>
#include <set>

#include "Game/KT1/Core/FxEvents/FxState.h"

using namespace KT1::Core::Fx;

namespace {

template<class T> void RoundTrip(KT1::Test::Case& testCase, std::set<int32_t>& types) {
	unsigned char in[sizeof(T)], back[sizeof(T)];
	for (size_t i = 0; i < sizeof(T); ++i) in[i] = static_cast<unsigned char>(i + T::TYPE + 1);
	T sent;
	std::memcpy(static_cast<void*>(&sent), in, sizeof(T));
	FxOutput out;
	out.Raise(sent);
	KT1_CHECK_EQ(out.events.size(), size_t(1));
	T got;
	KT1_CHECK(out.events[0].Payload(got));
	std::memcpy(back, static_cast<const void*>(&got), sizeof(T));
	KT1_CHECK_MSG(std::memcmp(in, back, sizeof(T)) == 0, "payload differs, type " + std::to_string(T::TYPE));
	KT1_CHECK_EQ(out.events[0].id, T::TYPE);
	KT1_CHECK(T::TYPE >= EVENT_BASE && T::TYPE < EVENT_BASE + EVENT_TYPES);
	KT1_CHECK_MSG(types.insert(T::TYPE).second, "two events share a type: " + std::to_string(T::TYPE));
}

template<class... T> void All(KT1::Test::Case& testCase) {
	std::set<int32_t> types;
	(RoundTrip<T>(testCase, types), ...);
	KT1_CHECK_EQ(static_cast<int32_t>(types.size()), EVENT_TYPES);
}

} // namespace

KT1_CASE("FXEV-ROUNDTRIP", "every event struct round trips through the output, each with its own type") {
	All<BuildSet, DeathSet, WreckImpulse, WreckThud, ScriptBlast, DepthHit, DepthIntercept, DepthLaunch, Scorch, Scale,
		ShotStart, ShotEnd, Impact, Death, Blast, CritDeath, FogDeath, Teleport, Crash, Remnant, Sacrifice, ShieldState,
		ShieldHit, CommanderBlast, StrategicLaunch, StrategicEnd, StrategicTactical, StrategicInterceptor,
		StrategicInterceptorPos, StrategicInterceptorEnd, TacticalLaunch, TacticalPos, TacticalEnd, MissileRedirected>(testCase);
}

KT1_CASE("FXEV-ORDER", "the output keeps the events in the order raised and Clear empties every list") {
	FxOutput out;
	out.Raise(Scale{2.0f});
	out.Raise(Crash{3, 4});
	out.spawns.push_back({});
	out.wreckWrites.push_back({});
	out.releaseWrecks.push_back(1);
	out.hideFeatures.push_back(2);
	out.showFeatures.push_back(3);
	Crash second;
	KT1_CHECK(!out.events[0].Payload(second));
	KT1_CHECK(out.events[1].Payload(second));
	KT1_CHECK_EQ(second.defId, 4);
	out.Clear();
	KT1_CHECK(out.events.empty() && out.spawns.empty() && out.wreckWrites.empty());
	KT1_CHECK(out.releaseWrecks.empty() && out.hideFeatures.empty() && out.showFeatures.empty());
}
